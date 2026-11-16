#include "wtlMediaToolkit.hpp"

#include <opencv2/opencv.hpp>
#include <gdiplus.h>
#include <wincodec.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#ifdef __MINGW32__
#include <initguid.h>
#endif
#include <mfapi.h>
#include <mfplay.h>
#include <shlwapi.h>
#include <wtlFileManagement.hpp>
#include <wtlPrintFormat.hpp>
#include <wtlRegex.hpp>
#include <wtlThrowError.hpp>

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace cv;
namespace fs = std::filesystem;

static std::wstring _audioToWidePath(
        const std::string& utf8Path,
        std::string& error)
{
    try
    {
        return fs::u8path(utf8Path).wstring();
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return L"";
    }
}

static std::string _audioToUtf8String(const std::wstring& text)
{
    if (text.empty())
    {
        return "";
    }

    const int length = WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
    if (length == 0)
    {
        return "Unknown error";
    }

    std::string result(length, '\0');
    WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            length,
            nullptr,
            nullptr);
    return result;
}

static std::wstring _audioCreateFileUrl(
        const std::wstring& path,
        std::string& error)
{
    std::vector<wchar_t> buffer(32768);
    DWORD length = static_cast<DWORD>(buffer.size());
    const HRESULT result = UrlCreateFromPathW(
            path.c_str(),
            buffer.data(),
            &length,
            0);
    if (FAILED(result))
    {
        error = "UrlCreateFromPathW failed: " + std::to_string(result);
        return L"";
    }

    return std::wstring(buffer.data());
}

// 仅供带缩放的匹配使用：读取本机系统缩放（96 DPI 记为 1.0）。
// 现有不带缩放的函数不依赖它，两者互不影响。
static float _getMediaScreenZoom()
{
    SetProcessDPIAware();
    HDC hdc = GetDC(nullptr);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);

    float scalingRatio = static_cast<float>(dpiX) / 96.0f;
    return static_cast<float>(static_cast<int>(scalingRatio * 1000 + 0.5f)) / 1000.0f;
}

// 生成临时目录名称时使用的本地时间字符串。
static string _getMediaNowTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime = *std::localtime(&time);

    char buffer[160];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}
namespace wtl
{
    using wtl::Color;
    using wtl::println;

    // 新增：区域截图函数
    static cv::Mat _captureScreenArea(int x, int y, int width, int height)
    {
        // 设置进程 DPI 感知级别
        SetProcessDPIAware();

        // 获取整个屏幕的设备上下文（DC）
        HDC hDC = GetDC(NULL);

        // 创建与屏幕兼容的内存设备上下文
        HDC memDC = CreateCompatibleDC(hDC);

        // 创建兼容位图（尺寸与指定区域相同）
        HBITMAP hBitmap = CreateCompatibleBitmap(hDC, width, height);

        // 将位图选入内存 DC
        SelectObject(memDC, hBitmap);

        // 将指定区域的屏幕内容复制到内存 DC
        BitBlt(memDC, 0, 0, width, height, hDC, x, y, SRCCOPY);

        // 创建 OpenCV Mat 对象存储截图（32 位 BGRA 格式）
        cv::Mat screenshot(height, width, CV_8UC4);

        // 配置位图信息头
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;  // 负数表示从上到下排列像素
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        // 从位图数据填充 Mat 对象
        GetDIBits(memDC, hBitmap, 0, height, screenshot.data, &bmi, DIB_RGB_COLORS);

        // 清理资源
        DeleteObject(hBitmap);
        DeleteDC(memDC);
        ReleaseDC(NULL, hDC);

        // 将 BGRA 转换为 BGR（移除 Alpha 通道）
        cv::cvtColor(screenshot, screenshot, cv::COLOR_BGRA2BGR);
        return screenshot;
    }

    // 函数：_captureScreen - 捕获当前屏幕截图并返回 OpenCV Mat 对象 //
    // origin 非空时输出虚拟屏幕左上角的屏幕坐标。截图以该点为 (0,0)，
    // 因此把截图坐标还原为屏幕绝对坐标（SetCursorPos / mouseClick 使用）时必须加上它。
    static cv::Mat _captureScreen(cv::Point* origin = nullptr)
    {
        // 设置进程 DPI 感知级别，确保截图与后续鼠标坐标处于同一物理像素空间
        SetProcessDPIAware();

        // 获取整个屏幕的设备上下文（DC）
        HDC hDC = GetDC(NULL);
        // 获取虚拟屏幕的尺寸（多显示器情况下）
        int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        int x = GetSystemMetrics(SM_XVIRTUALSCREEN);  // 虚拟屏幕左上角 X 坐标
        int y = GetSystemMetrics(SM_YVIRTUALSCREEN);  // 虚拟屏幕左上角 Y 坐标

        if (origin != nullptr)
            *origin = cv::Point(x, y);

        // 创建与屏幕兼容的内存设备上下文
        HDC memDC = CreateCompatibleDC(hDC);
        // 创建兼容位图（尺寸与屏幕相同）
        HBITMAP hBitmap = CreateCompatibleBitmap(hDC, width, height);
        // 将位图选入内存 DC
        SelectObject(memDC, hBitmap);

        // 将屏幕内容复制到内存 DC（从屏幕 DC 到内存 DC）
        BitBlt(memDC, 0, 0, width, height, hDC, x, y, SRCCOPY);

        // 创建 OpenCV Mat 对象存储截图（32 位 BGRA 格式）
        cv::Mat screenshot(height, width, CV_8UC4);
        // 配置位图信息头
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));  // 清空结构体
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;  // 负数表示从上到下排列像素
        bmi.bmiHeader.biPlanes = 1;        // 颜色平面数（必须为 1）
        bmi.bmiHeader.biBitCount = 32;     // 每像素 32 位（BGRA）
        bmi.bmiHeader.biCompression = BI_RGB;  // 无压缩
        // 从位图数据填充 Mat 对象
        GetDIBits(memDC, hBitmap, 0, height, screenshot.data, &bmi, DIB_RGB_COLORS);

        // 清理资源
        DeleteObject(hBitmap);  // 删除位图对象
        DeleteDC(memDC);        // 删除内存 DC
        ReleaseDC(NULL, hDC);   // 释放屏幕 DC

        // 将 BGRA 转换为 BGR（移除 Alpha 通道）
        cv::cvtColor(screenshot, screenshot, cv::COLOR_BGRA2BGR);
        return screenshot;
    }

    // =================== Unicode 安全读写（UTF-8 路径） =================== //
    // cv::imread / cv::imwrite 在 Windows 上把 std::string 按 ANSI(GBK) 解释，中文路径会乱码。
    // 这里改为自己用 _wfopen 以 UTF-16 打开文件，再由 OpenCV 从内存 imdecode / imencode，
    // 彻底支持 Unicode 路径。约定：库对外传入的路径一律为 UTF-8。

    // UTF-8 -> UTF-16（统一使用官方 fs::path，无需手动 MultiByteToWideChar）
    static std::wstring _utf8ToWstring(const std::string& utf8)
    {
        return fs::u8path(utf8).wstring();
    }

    // Unicode 安全读取图片
    static cv::Mat _imreadUnicode(const std::string& utf8Path, int flags = cv::IMREAD_COLOR)
    {
        std::wstring wpath = _utf8ToWstring(utf8Path);
        if (wpath.empty()) return cv::Mat();

        FILE* fp = _wfopen(wpath.c_str(), L"rb");
        if (!fp) return cv::Mat();

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        std::vector<unsigned char> buf(size > 0 ? size : 0);
        if (size > 0) fread(buf.data(), 1, size, fp);
        fclose(fp);

        return cv::imdecode(buf, flags);
    }

    // Unicode 安全写入图片（按路径扩展名决定格式，无扩展名默认 .png）
    static bool _imwriteUnicode(const std::string& utf8Path, const cv::Mat& img)
    {
        std::string ext = ".png";
        size_t dot = utf8Path.find_last_of('.');
        size_t slash = utf8Path.find_last_of("/\\");
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
            ext = utf8Path.substr(dot);

        std::vector<unsigned char> buf;
        if (!cv::imencode(ext, img, buf)) return false;

        std::wstring wpath = _utf8ToWstring(utf8Path);
        if (wpath.empty()) return false;

        FILE* fp = _wfopen(wpath.c_str(), L"wb");
        if (!fp) return false;
        fwrite(buf.data(), 1, buf.size(), fp);
        fclose(fp);
        return true;
    }

    // =================== 模板匹配（灰度，无需传入缩放） =================== //
    // 说明：跨设备（不同系统缩放）识别因缺陷临时移除，当前仅在采集模板的同一台电脑 /
    // 同一系统缩放下按原始尺寸匹配；调用方只需提供图片路径。
    struct ScaledMatch
    {
        double maxVal;          // 最佳匹配值
        cv::Point maxLoc;       // 最佳匹配左上角坐标（屏幕物理像素）
        cv::Size matchedSize;   // 命中时模板缩放后的尺寸
    };

    // 候选尺度：跨设备识别临时移除，仅保留 1.0（原尺寸，同机采集）。
    // 需要恢复跨设备识别时，把这里改回覆盖 0.5 ~ 2.0 的多尺度阶梯即可。
    static const std::vector<double>& _scaleCandidates()
    {
        static const std::vector<double> scales = { 1.00 };
        return scales;
    }

    // 把 n 个互相独立的任务并行派发到若干工作线程；任务数少或单核时退化为串行。
    template <typename Fn>
    static void _runParallel(int n, Fn&& fn)
    {
        if (n <= 1)
        {
            for (int i = 0; i < n; ++i) fn(i);
            return;
        }

        unsigned hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4;
        int workers = static_cast<int>(std::min<unsigned>(hw, static_cast<unsigned>(n)));
        if (workers <= 1)
        {
            for (int i = 0; i < n; ++i) fn(i);
            return;
        }

        std::atomic<int> next{0};
        std::vector<std::thread> pool;
        pool.reserve(workers);
        for (int t = 0; t < workers; ++t)
            pool.emplace_back([&] {
                int i;
                while ((i = next.fetch_add(1)) < n)
                {
                    try { fn(i); } catch (...) { /* 单个尺度失败不影响其它尺度 */ }
                }
            });
        for (std::thread& th : pool) th.join();
    }

    // 转灰度：模板匹配用单通道比三通道快数倍，UI 模板定位精度基本不受影响。
    static cv::Mat _toGray(const cv::Mat& img)
    {
        if (img.channels() == 1)
            return img;
        cv::Mat gray;
        cv::cvtColor(img, gray, img.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
        return gray;
    }

    // 单个尺度在整屏上的最佳匹配；未命中或模板比屏幕大时 maxVal = -1。
    static ScaledMatch _matchAtScale(const cv::Mat& grayScreen, const cv::Mat& grayTpl, double s)
    {
        ScaledMatch m;
        m.maxVal = -1.0;
        m.maxLoc = cv::Point(0, 0);
        m.matchedSize = grayTpl.size();
        if (s <= 0.0)
            return m;

        cv::Mat tpl;
        cv::resize(grayTpl, tpl, cv::Size(), s, s, cv::INTER_LINEAR);
        if (tpl.empty() || tpl.rows > grayScreen.rows || tpl.cols > grayScreen.cols)
            return m;  // 缩放后模板比屏幕大，跳过该尺度

        cv::Mat result;
        cv::matchTemplate(grayScreen, tpl, result, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        m.maxVal = maxVal;
        m.maxLoc = maxLoc;
        m.matchedSize = tpl.size();
        return m;
    }

    static ScaledMatch _matchTemplateScaled(const cv::Mat& screenshot, const cv::Mat& templateImg)
    {
        ScaledMatch best;
        best.maxVal = -1.0;
        best.maxLoc = cv::Point(0, 0);
        best.matchedSize = templateImg.size();

        // 灰度匹配（候选尺度当前仅 1.0）。
        cv::Mat grayScreen = _toGray(screenshot);
        cv::Mat grayTpl = _toGray(templateImg);
        const std::vector<double>& scales = _scaleCandidates();

        ScaledMatch miss;
        miss.maxVal = -1.0;
        miss.maxLoc = cv::Point(0, 0);
        miss.matchedSize = grayTpl.size();
        std::vector<ScaledMatch> results(scales.size(), miss);

        _runParallel(static_cast<int>(scales.size()), [&](int i) {
            results[i] = _matchAtScale(grayScreen, grayTpl, scales[i]);
        });

        for (const ScaledMatch& m : results)
            if (m.maxVal > best.maxVal)
                best = m;
        return best;
    }

    // 带手动缩放的匹配（供 ...Zoom 系列函数使用，不改动上面的同机匹配路径）。
    // templateZoom：模板采集时那台电脑的系统缩放（1.5 = 150%）。
    // 把模板按 本机缩放 / templateZoom 归一到本机物理像素，再在基准附近做小幅微调。
    static ScaledMatch _matchTemplateAtZoom(const cv::Mat& screenshot, const cv::Mat& templateImg, double templateZoom)
    {
        cv::Mat grayScreen = _toGray(screenshot);
        cv::Mat grayTpl = _toGray(templateImg);

        double base = 1.0;
        if (templateZoom > 0.0)
            base = static_cast<double>(_getMediaScreenZoom()) / templateZoom;  // 非法入参退化为原尺寸

        // 基准附近 ±6%（步进 2%），容忍非整数缩放与字体渲染的像素误差。
        const double rels[] = { 0.94, 0.96, 0.98, 1.00, 1.02, 1.04, 1.06 };
        const int n = static_cast<int>(sizeof(rels) / sizeof(rels[0]));

        ScaledMatch miss;
        miss.maxVal = -1.0;
        miss.maxLoc = cv::Point(0, 0);
        miss.matchedSize = grayTpl.size();
        std::vector<ScaledMatch> results(n, miss);

        _runParallel(n, [&](int i) {
            results[i] = _matchAtScale(grayScreen, grayTpl, base * rels[i]);
        });

        ScaledMatch best = miss;
        for (const ScaledMatch& m : results)
            if (m.maxVal > best.maxVal)
                best = m;
        return best;
    }

    // 获取图像坐标函数,将此函数移到图像操作中 //
    ImagePosition ImageTools::getImagePosition(const std::string &filePath, double threshold,
                                               bool notException)
    {
        if (!notException)
        {
            ImagePosition position;
            // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标
            SetProcessDPIAware();

            // 捕获屏幕截图
            cv::Point origin;  // 虚拟屏幕左上角，用于把截图坐标还原为屏幕绝对坐标
            cv::Mat screenshot = _captureScreen(&origin);

            // 加载模板图像（需确保 "img.png" 位于工作目录或提供绝对路径）
            cv::Mat templateImg = _imreadUnicode(filePath);
            if (templateImg.empty())
            {
                wtl::ThrowError::showError("The template image cannot be loaded!");
            }

            // 灰度模板匹配（无需传入缩放）
            ScaledMatch m = _matchTemplateScaled(screenshot, templateImg);

            if (m.maxVal >= threshold)
            {
                // 计算目标中心坐标（使用命中尺度下的模板尺寸）
                position.x = origin.x + m.maxLoc.x + m.matchedSize.width / 2;
                position.y = origin.y + m.maxLoc.y + m.matchedSize.height / 2;
                position.result = true;
                return position;
            }
            else
            {
                wtl::ThrowError::showError("Image not found!");
            }
        }
        else
        {
            ImagePosition position;
            // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标
            SetProcessDPIAware();

            // 捕获屏幕截图
            cv::Point origin;  // 虚拟屏幕左上角，用于把截图坐标还原为屏幕绝对坐标
            cv::Mat screenshot = _captureScreen(&origin);

            // 加载模板图像（需确保 "img.png" 位于工作目录或提供绝对路径）
            cv::Mat templateImg = _imreadUnicode(filePath);
            if (templateImg.empty())
            {
                position.result = false;
                position.x = -1;
                position.y = -1;
                return position;
            }

            // 灰度模板匹配（无需传入缩放）
            ScaledMatch m = _matchTemplateScaled(screenshot, templateImg);

            if (m.maxVal >= threshold)
            {
                position.x = origin.x + m.maxLoc.x + m.matchedSize.width / 2;
                position.y = origin.y + m.maxLoc.y + m.matchedSize.height / 2;
                position.result = true;
                return position;
            }
            else
            {
                position.result = false;
                position.x = -1;
                position.y = -1;
                return position;
            }
        }
    }

    // 获取图像坐标函数 - 返回所有匹配位置
    std::vector<ImagePosition> ImageTools::getImagePositionVec(const std::string &filePath, double threshold,
                                                               bool notException, int maxMatches)
    {
        std::vector<ImagePosition> positions;

        // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标
        SetProcessDPIAware();

        // 捕获屏幕截图
        cv::Point origin;  // 虚拟屏幕左上角，用于把截图坐标还原为屏幕绝对坐标
        cv::Mat screenshot = _captureScreen(&origin);

        // 加载模板图像
        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            if (!notException)
            {
                wtl::ThrowError::showError("The template image cannot be loaded!");
            }
            return positions;  // 返回空vector
        }

        // 收集各尺度下所有满足阈值的匹配（匹配值 + 中心坐标）
        struct RawMatch { double val; cv::Point center; };
        std::vector<std::vector<RawMatch>> perScale(_scaleCandidates().size());

        // 灰度匹配（候选尺度当前仅 1.0）。
        cv::Mat grayScreen = _toGray(screenshot);
        cv::Mat grayTpl = _toGray(templateImg);
        const std::vector<double>& scales = _scaleCandidates();

        _runParallel(static_cast<int>(scales.size()), [&](int idx) {
            double s = scales[idx];
            if (s <= 0.0)
                return;

            cv::Mat tpl;
            cv::resize(grayTpl, tpl, cv::Size(), s, s, cv::INTER_LINEAR);
            if (tpl.empty() || tpl.rows > grayScreen.rows || tpl.cols > grayScreen.cols)
                return;

            cv::Mat result;
            cv::matchTemplate(grayScreen, tpl, result, cv::TM_CCOEFF_NORMED);

            std::vector<RawMatch>& out = perScale[idx];
            // 逐个找该尺度下所有 >= threshold 的匹配
            while (true)
            {
                double minVal, maxVal;
                cv::Point minLoc, maxLoc;
                cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
                if (maxVal < threshold)
                    break;

                int cx = origin.x + maxLoc.x + tpl.cols / 2;
                int cy = origin.y + maxLoc.y + tpl.rows / 2;
                out.push_back({maxVal, cv::Point(cx, cy)});

                // 掩码掉已找到的区域，避免重复
                int maskRadius = tpl.cols / 2;
                cv::Rect maskRect(
                        std::max(0, maxLoc.x - maskRadius),
                        std::max(0, maxLoc.y - maskRadius),
                        std::min(tpl.cols + maskRadius * 2, result.cols - std::max(0, maxLoc.x - maskRadius)),
                        std::min(tpl.rows + maskRadius * 2, result.rows - std::max(0, maxLoc.y - maskRadius))
                );
                cv::Mat roi = result(maskRect);
                roi.setTo(-1);
            }
        });

        std::vector<RawMatch> raw;
        for (const std::vector<RawMatch>& v : perScale)
            raw.insert(raw.end(), v.begin(), v.end());

        // 按匹配值降序，去重（中心过近的只留一个），再截断到 maxMatches
        std::sort(raw.begin(), raw.end(), [](const RawMatch& a, const RawMatch& b){ return a.val > b.val; });

        if (maxMatches <= 0)
            maxMatches = INT_MAX;

        int dedupRadius = templateImg.cols / 2;
        for (const auto& r : raw)
        {
            bool dup = false;
            for (const auto& p : positions)
            {
                int dx = p.x - r.center.x;
                int dy = p.y - r.center.y;
                if (dx * dx + dy * dy < dedupRadius * dedupRadius)
                {
                    dup = true;
                    break;
                }
            }
            if (dup)
                continue;

            ImagePosition position;
            position.x = r.center.x;
            position.y = r.center.y;
            position.result = true;
            positions.push_back(position);

            if ((int)positions.size() >= maxMatches)
                break;
        }

        if (positions.empty() && !notException)
        {
            wtl::ThrowError::showError("Image not found!");
        }

        return positions;
    }

    // 新增：在指定区域内获取图像坐标 //
    ImagePosition ImageTools::getSpecifiedAreaImagePosition(int x1, int y1, int x2, int y2,
                                                            const string& filePath, double threshold,
                                                            bool notException)
    {
        if (!notException)
        {
            ImagePosition position;
            // 设置进程 DPI 感知级别
            SetProcessDPIAware();

            // 计算区域宽度和高度
            int width = x2 - x1;
            int height = y2 - y1;

            if (width <= 0 || height <= 0)
            {
                wtl::ThrowError::showError("Invalid specified area coordinates!");
            }

            // 捕获指定区域的屏幕截图
            cv::Mat screenshot = _captureScreenArea(x1, y1, width, height);

            // 加载模板图像
            cv::Mat templateImg = _imreadUnicode(filePath);
            if (templateImg.empty())
            {
                wtl::ThrowError::showError("The template image cannot be loaded!");
            }

            // 灰度模板匹配（无需传入缩放）
            ScaledMatch m = _matchTemplateScaled(screenshot, templateImg);

            if (m.maxVal >= threshold)
            {
                // 局部坐标 -> 全局屏幕坐标
                int globalX = x1 + m.maxLoc.x + m.matchedSize.width / 2;
                int globalY = y1 + m.maxLoc.y + m.matchedSize.height / 2;

                position.x = globalX;
                position.y = globalY;
                position.result = true;
                return position;
            }
            else
            {
                wtl::ThrowError::showError("Image not found in the specified area!");
            }
        }
        else
        {
            ImagePosition position;
            // 设置进程 DPI 感知级别
            SetProcessDPIAware();

            // 计算区域宽度和高度
            int width = x2 - x1;
            int height = y2 - y1;

            if (width <= 0 || height <= 0)
            {
                position.result = false;
                position.x = -1;
                position.y = -1;
                return position;
            }

            // 捕获指定区域的屏幕截图
            cv::Mat screenshot = _captureScreenArea(x1, y1, width, height);

            // 加载模板图像
            cv::Mat templateImg = _imreadUnicode(filePath);
            if (templateImg.empty())
            {
                position.result = false;
                position.x = -1;
                position.y = -1;
                return position;
            }

            // 灰度模板匹配（无需传入缩放）
            ScaledMatch m = _matchTemplateScaled(screenshot, templateImg);

            if (m.maxVal >= threshold)
            {
                int globalX = x1 + m.maxLoc.x + m.matchedSize.width / 2;
                int globalY = y1 + m.maxLoc.y + m.matchedSize.height / 2;

                position.x = globalX;
                position.y = globalY;
                position.result = true;
                return position;
            }
            else
            {
                position.result = false;
                position.x = -1;
                position.y = -1;
                return position;
            }
        }
    }

    bool ImageTools::findSpecifiedSimilarImage(int x1, int y1, int x2, int y2, const std::string &filePath,
                                               double threshold)
    {

        // 设置进程 DPI 感知级别
        SetProcessDPIAware();

        // 计算区域宽度和高度
        int width = x2 - x1;
        int height = y2 - y1;

        // 检查区域有效性
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        // 捕获指定区域的屏幕截图
        cv::Mat screenshot = _captureScreenArea(x1, y1, width, height);
        if (screenshot.empty())
        {
            return false;
        }

        // 加载模板图像
        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            return false;
        }

        // 灰度模板匹配（无需传入缩放）
        ScaledMatch m = _matchTemplateScaled(screenshot, templateImg);

        // 判断匹配结果是否满足阈值
        return (m.maxVal >= threshold);
    }

    bool ImageTools::findSimilarImage(const string& filePath, double threshold)
    {
        // 设置进程 DPI 感知级别
        SetProcessDPIAware();

        // 捕获屏幕截图
        cv::Mat screenshot = _captureScreen();
        if (screenshot.empty()) {
            return false;
        }

        // 加载模板图像
        cv::Mat templateImg = _imreadUnicode(filePath, cv::IMREAD_COLOR);
        if (templateImg.empty()) {
            return false;
        }

        // 灰度模板匹配（无需传入缩放）
        ScaledMatch m = _matchTemplateScaled(screenshot, templateImg);

        // 判断匹配结果是否达到阈值
        return (m.maxVal >= threshold);
    }

    bool ImageTools::findSimilarImage(const string& templatePath, const string& comparePath, double threshold)
    {
        // 加载模板图像
        cv::Mat templateImg = _imreadUnicode(templatePath);
        if (templateImg.empty())
        {
            return false;
        }

        // 加载待比较的图像
        cv::Mat compareImg = _imreadUnicode(comparePath);
        if (compareImg.empty())
        {
            return false;
        }

        // 如果模板比待比较图大，则无法匹配
        if (templateImg.rows > compareImg.rows || templateImg.cols > compareImg.cols)
        {
            return false;
        }

        // 执行模板匹配
        cv::Mat result;
        cv::matchTemplate(compareImg, templateImg, result, cv::TM_CCOEFF_NORMED);

        // 获取匹配结果的最大值
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

        // 判断是否达到阈值
        if (maxVal >= threshold)
        {
            return true;
        }

        return false;
    }

    // 设置图片格式 //
    bool ImageTools::setImageFormat(const string& imgPath, const string& targetImageFormat)
    {
        // 1. 读取图片（支持JPG/PNG/BMP等）
        Mat image = _imreadUnicode(imgPath);

        // 2. 检查是否读取成功
        if (image.empty())
        {
            std::cerr << "Image reading failed!\n";
            return false;
        }

        // 3. 转换并保存为不同格式
        bool success = _imwriteUnicode(targetImageFormat, image);  // 转为PNG

        // 检查保存结果
        if (!success)
        {
            std::cerr << "Image save failed!\n";
            return false;
        }
        return true;
    }


    // 全局截图 //
    bool ImageTools::_saveBitmapToFile(HBITMAP hBitmap, const wchar_t* filename)
    {
        HRESULT hr = S_OK;

        IWICImagingFactory* pFactory = nullptr;
        IWICBitmap* pWICBitmap = nullptr;
        IWICStream* pStream = nullptr;
        IWICBitmapEncoder* pEncoder = nullptr;
        IWICBitmapFrameEncode* pFrameEncode = nullptr;
        IPropertyBag2* pPropertybag = nullptr;

        // 创建 WIC 工厂
        hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&pFactory)
        );

        if (FAILED(hr)) return false;

        // 将 HBITMAP 转换为 WIC 位图
        hr = pFactory->CreateBitmapFromHBITMAP(
                hBitmap,
                nullptr,
                WICBitmapUseAlpha,
                &pWICBitmap
        );

        if (FAILED(hr)) {
            pFactory->Release();
            return false;
        }

        // 创建文件流
        hr = pFactory->CreateStream(&pStream);
        if (FAILED(hr)) goto CLEANUP;

        hr = pStream->InitializeFromFilename(filename, GENERIC_WRITE);
        if (FAILED(hr)) goto CLEANUP;

        // 创建 PNG 编码器
        hr = pFactory->CreateEncoder(
                GUID_ContainerFormatPng,
                nullptr,
                &pEncoder
        );

        if (FAILED(hr)) goto CLEANUP;

        hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
        if (FAILED(hr)) goto CLEANUP;

        // 创建新帧
        hr = pEncoder->CreateNewFrame(&pFrameEncode, &pPropertybag);
        if (FAILED(hr)) goto CLEANUP;

        hr = pFrameEncode->Initialize(pPropertybag);
        if (FAILED(hr)) goto CLEANUP;

        // 设置帧尺寸
        UINT width, height;
        pWICBitmap->GetSize(&width, &height);
        hr = pFrameEncode->SetSize(width, height);
        if (FAILED(hr)) goto CLEANUP;

        // 获取并设置像素格式
        WICPixelFormatGUID pixelFormat;
        pWICBitmap->GetPixelFormat(&pixelFormat);
        hr = pFrameEncode->SetPixelFormat(&pixelFormat);
        if (FAILED(hr)) goto CLEANUP;

        // 写入位图数据
        hr = pFrameEncode->WriteSource(pWICBitmap, nullptr);
        if (FAILED(hr)) goto CLEANUP;

        // 提交更改
        hr = pFrameEncode->Commit();
        if (FAILED(hr)) goto CLEANUP;

        hr = pEncoder->Commit();
        if (FAILED(hr)) goto CLEANUP;

        CLEANUP:
        if (pPropertybag) pPropertybag->Release();
        if (pFrameEncode) pFrameEncode->Release();
        if (pEncoder) pEncoder->Release();
        if (pStream) pStream->Release();
        if (pWICBitmap) pWICBitmap->Release();
        if (pFactory) pFactory->Release();

        return SUCCEEDED(hr);
    }

    // 截图功能 //
    bool ImageTools::captureScreen(const std::string& imageName)
    {
        // 初始化COM库（确保在程序启动时调用一次）
        static bool comInitialized = false;
        if (!comInitialized)
        {
            CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            comInitialized = true;
        }

        HDC hdcScreen = nullptr;
        HDC hdcMem = nullptr;
        HBITMAP hBitmap = nullptr;

        std::string fullName = imageName + ".png";
        std::wstring fileName = fs::u8path(fullName).wstring();
        const wchar_t* filename = fileName.c_str();

        SetProcessDPIAware();
        //    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

        int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        hdcScreen = GetDC(nullptr);
        hdcMem = CreateCompatibleDC(hdcScreen);
        hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);

        if (!hBitmap)
        {
            DeleteDC(hdcMem);
            ReleaseDC(nullptr, hdcScreen);
            std::cerr << "Image saving failed!\n";
            return false;
        }

        SelectObject(hdcMem, hBitmap);
        BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);

        if (!_saveBitmapToFile(hBitmap, filename))
        {
            std::cerr << "Image saving failed!\n";
            return false;
        }

        // 正确释放资源
        ReleaseDC(nullptr, hdcScreen);
        DeleteDC(hdcMem);
        DeleteObject(hBitmap);
        isCaptureScreen = true;
        return true;
    }

    bool ImageTools::captureScreen(const string& imageName, const string& targetImageSavePath)
    {
        // 初始化COM库（确保在程序启动时调用一次）
        static bool comInitialized = false;
        if (!comInitialized)
        {
            CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            comInitialized = true;
        }

        HDC hdcScreen = nullptr;
        HDC hdcMem = nullptr;
        HBITMAP hBitmap = nullptr;

        std::string fullName = imageName + ".png";
        std::wstring fileName = fs::u8path(fullName).wstring();
        const wchar_t* filename = fileName.c_str();

        SetProcessDPIAware();
        //    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

        int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        hdcScreen = GetDC(nullptr);
        hdcMem = CreateCompatibleDC(hdcScreen);
        hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);

        if (!hBitmap)
        {
            DeleteDC(hdcMem);
            ReleaseDC(nullptr, hdcScreen);
            std::cerr << "Image saving failed!\n";
            return false;
        }

        SelectObject(hdcMem, hBitmap);
        BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);

        wstring savePath = fs::u8path(targetImageSavePath).wstring() + L"/";
        savePath += fileName;
        if (!_saveBitmapToFile(hBitmap, savePath.c_str()))
        {
            std::cerr << "Image saving failed!\n";
            return false;
        }

        // 正确释放资源
        ReleaseDC(nullptr, hdcScreen);
        DeleteDC(hdcMem);
        DeleteObject(hBitmap);
        isCaptureScreen = true;
        return true;
    }

    // 获取路径图片名称 //
    string ImageTools::_getImageNameFunc(const string& imgPath)
    {
        // 输出原本图片名称+格式 //
        string str = imgPath;
        // 参数获取后缀名称 //
        std::regex pattern(R"((\w+)\.(\w+))");
        // 用于存储匹配结果的std::smatch对象
        std::smatch match;

        // 局部搜索 //
        if (std::regex_search(str, match, pattern))
        {
            // 获取捕获组，[0]是整个匹配的结果，[1]开始是捕获组 //
            string str = match[1].str();
            string str2 = match[2].str();
            // 组合名称 //
            string fileName = str + "." + str2;
            cout << fileName << endl;
            return fileName;
        }
        // 匹配失败返回空字符串 //
        return "";
    }

    string ImageTools::_getImageNameFunc(const string& imgPath, const string& name)
    {
        // 仅仅输出图片格式 //
        string str = imgPath;
        // 参数获取后缀名称 //
        std::regex pattern(R"((\w+)\.(\w+))");
        // 用于存储匹配结果的std::smatch对象
        std::smatch match;

        // 检查是否为空名称 //
        if (!name.empty())
        {
            // 局部搜索 //
            if (std::regex_search(str, match, pattern))
            {
                // 获取捕获组，[0]是整个匹配的结果，[1]开始是捕获组 //
                string str2 = match[2].str();
                // 组合名称 //
                string fileName = name + "." + str2;
                return fileName;
            }
        }
        else
        {
            string fileName = _getImageNameFunc(imgPath);
            return fileName;
        }
        // 匹配失败返回空字符串 //
        return "";
    }

    // 设置图片尺寸 //
    bool ImageTools::setImageSize(const string& imgPath, float width, float height, const string& targetImageSavePath, const string& outPutName)
    {
        // 保存图片大小 //
        // 读取图像 //
        cv::Mat src = _imreadUnicode(imgPath);
        if (src.empty())
        {
            std::cerr << "image not find!" << std::endl;
            return false;
        }

        // 调整尺寸为 800x600，使用双三次插值 //
        cv::Mat dst;
        cv::resize(src, dst, cv::Size(width, height), 0, 0, cv::INTER_CUBIC);

        // 保存结果 //
        string saveImageName = targetImageSavePath + "/" + _getImageNameFunc(imgPath, outPutName);
        _imwriteUnicode(saveImageName, dst);
        return true;
    }

    ImageSize ImageTools::getImageSize(const string &filePath, bool outPutFlag)
    {
        ImageSize img_size;
        // 读取图片 //
        cv::Mat image = _imreadUnicode(filePath);

        // 检查图片是否成功加载 //
        if (image.empty())
        {
            std::cout << "Error: Unable to read the image." << std::endl;
            img_size.x = 0;
            img_size.y = 0;
            return img_size;
        }
        int channels = image.channels();
        cv::Size imgSize = image.size();
        img_size.x = imgSize.width;
        img_size.y = imgSize.height;
        img_size.channels = channels;

        if (outPutFlag)
        {
            wtl::println("image size: (width: {}, height: {}) channels: {}", imgSize.width, imgSize.height, channels);
        }

        return img_size;
    }


    bool ImageTools::setImageSize(const string& imgPath, float width, float height)
    {
        // 读取图像 //
        cv::Mat src = _imreadUnicode(imgPath);
        if (src.empty())
        {
            std::cerr << "image not find!" << std::endl;
            return false;
        }

        // 调整尺寸为 800x600，使用双三次插值 //
        cv::Mat dst;
        cv::resize(src, dst, cv::Size(width, height), 0, 0, cv::INTER_CUBIC);

        // 保存结果 //
        string saveImageName = _getImageNameFunc(imgPath);
        _imwriteUnicode(saveImageName, dst);
        return true;
    }


    bool ImageTools::setImageSize(const string& imgPath, float width, float height, const string& outPutName)
    {
        // 读取图像 //
        cv::Mat src = _imreadUnicode(imgPath);
        if (src.empty())
        {
            std::cerr << "image not find!" << std::endl;
            return false;
        }

        // 调整尺寸为 800x600，使用双三次插值 //
        cv::Mat dst;
        cv::resize(src, dst, cv::Size(width, height), 0, 0, cv::INTER_CUBIC);

        // 保存结果 //
        string saveImageName = _getImageNameFunc(imgPath, outPutName);
        _imwriteUnicode(saveImageName, dst);
        return true;
    }

    ImageRect ImageTools::getImageRect(const string& filePath, double threshold, bool notException)
    {
        ImageRect rect;
        rect.result = false;
        rect.topX = -1;
        rect.topY = -1;
        rect.btnX = -1;
        rect.btnY = -1;

        SetProcessDPIAware();

        // 捕获屏幕并加载模板
        cv::Point origin;  // 虚拟屏幕左上角，用于把截图坐标还原为屏幕绝对坐标
        cv::Mat screenshot = _captureScreen(&origin);
        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            if (!notException)
            {
                wtl::ThrowError::showError("The template image cannot be loaded!");
            }
            return rect;
        }

        // 灰度模板匹配（无需传入缩放）
        ScaledMatch m = _matchTemplateScaled(screenshot, templateImg);

        if (m.maxVal >= threshold)
        {
            // 直接使用命中尺度下的模板尺寸计算矩形
            rect.topX = origin.x + m.maxLoc.x;
            rect.topY = origin.y + m.maxLoc.y;
            rect.btnX = origin.x + m.maxLoc.x + m.matchedSize.width;
            rect.btnY = origin.y + m.maxLoc.y + m.matchedSize.height;
            rect.result = true;
        }
        else if (!notException)
        {
            wtl::ThrowError::showError("Image not found!");
        }

        return rect;
    }

    // 模板图片从主图片中获取矩形位置 //
    ImageRect ImageTools::getImageRect(const string& templatePath, const string &filePath, double threshold, bool notException)
    {
        ImageRect rect;
        rect.result = false;
        rect.topX = -1;
        rect.topY = -1;
        rect.btnX = -1;
        rect.btnY = -1;

        try
        {
            // 读取模板图片和原图片
            Mat templateImg = _imreadUnicode(templatePath, IMREAD_COLOR);
            Mat sourceImg = _imreadUnicode(filePath, IMREAD_COLOR);

            if (templateImg.empty() || sourceImg.empty())
            {
                if (!notException)
                {
                    wtl::ThrowError::showError("Failed to load template or source image");
                }
                return rect;
            }

            // 检查模板尺寸是否大于原图
            if (templateImg.rows > sourceImg.rows || templateImg.cols > sourceImg.cols)
            {
                if (!notException)
                {
                    wtl::ThrowError::showError("Template image is larger than source image");
                }
                return rect;
            }

            // 执行模板匹配
            Mat result;
            matchTemplate(sourceImg, templateImg, result, TM_CCOEFF_NORMED);

            // 查找最佳匹配位置
            double minVal, maxVal;
            Point minLoc, maxLoc;
            minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

            // 检查匹配度是否达到阈值
            if (maxVal >= threshold)
            {
                rect.topX = maxLoc.x;
                rect.topY = maxLoc.y;
                rect.btnX = maxLoc.x + templateImg.cols;
                rect.btnY = maxLoc.y + templateImg.rows;
                rect.result = true;
            }
        }
        catch (const std::exception& e)
        {
            if (!notException)
            {
                throw;
            }
        }
        catch (...)
        {
            if (!notException)
            {
                throw;
            }
        }

        return rect;
    }



    // 区域截图 //
    int ImageTools::_getEncoderClsid(const WCHAR* format, CLSID* pClsid)
    {
        UINT num = 0;          // 编码器数量
        UINT size = 0;         // 编码器信息大小
        Gdiplus::GetImageEncodersSize(&num, &size);
        if (size == 0) return -1;  // 无可用编码器

        // 分配内存并获取编码器列表
        Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)malloc(size);
        if (!pImageCodecInfo) return -1;  // 内存分配失败
        Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

        // 遍历匹配MIME类型
        for (UINT j = 0; j < num; ++j) {
            if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
                *pClsid = pImageCodecInfo[j].Clsid;
                free(pImageCodecInfo);
                return j;  // 返回找到的索引
            }
        }

        free(pImageCodecInfo);
        return -1;  // 未找到匹配编码器
    }

    bool ImageTools::drawRectOnImage(const std::string &imgPath,const std::string &outPutName,  int topX, int topY, int btnX, int btnY)
    {
        // 在原图上画出矩形框并保存
        cv::Mat img = _imreadUnicode(imgPath);
        if (!img.empty())
        {
            // 在检测到的模板匹配区域画矩形框
            cv::rectangle(img,
                          cv::Point(topX, topY),
                          cv::Point(btnX, btnY),
                          cv::Scalar(0, 0, 255),  // 红色框
                          2);                     // 线宽2像素

            // 保存结果
            _imwriteUnicode(outPutName, img);
            return true;
        }
        else
        {
            wtl::println(wtl::Color("red"),"Failed to draw the rectangle");
            return false;
        }
    }


    // 功能函数 //
    void ImageTools::captureAreaScreen(const string& saveName, int x1, int y1, int x2, int y2)
    {
        // 初始化GDI+
        Gdiplus::GdiplusStartupInput input;
        ULONG_PTR token;
        Gdiplus::GdiplusStartup(&token, &input, nullptr);

        // 获取屏幕DC
        HDC screenDC = GetDC(nullptr);
        HDC memDC = CreateCompatibleDC(screenDC);
        int width = x2 - x1, height = y2 - y1;

        // 复制矩形区域
        HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, width, height);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBitmap);
        BitBlt(memDC, 0, 0, width, height, screenDC, x1, y1, SRCCOPY);

        // 保存为PNG
        CLSID pngClsid;
        if (_getEncoderClsid(L"image/png", &pngClsid) != -1) {
            Gdiplus::Bitmap bitmap(hBitmap, nullptr);
            wstring fileName = fs::u8path(saveName).wstring();
            bitmap.Save(fileName.c_str(), &pngClsid, nullptr);
        }

        // 清理资源
        SelectObject(memDC, oldBmp);
        DeleteObject(hBitmap);
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        Gdiplus::GdiplusShutdown(token);
    }
    // =================== 去水印 =================== //
    static cv::Mat _removeWatermarkByRect(const cv::Mat& src, const cv::Rect& rect)
    {
        /*
         * @brief 根据矩形区域去除水印（核心处理函数）
         * @param src 输入原图像
         * @param rect 水印所在矩形区域
         * @return 处理后的图像
         */
        // 检查输入有效性
        if(src.empty()||rect.width<=0||rect.height<=0)return src.clone();

        // 确保矩形区域不超出图像边界
        cv::Rect r=rect;
        r.x=std::max(0,r.x);
        r.y=std::max(0,r.y);
        r.width=std::min(r.width,src.cols-r.x);
        r.height=std::min(r.height,src.rows-r.y);
        if(r.width<=0||r.height<=0)return src.clone();

        // 创建掩膜：水印区域设为白色(255)，其余为黑色(0)
        cv::Mat mask=cv::Mat::zeros(src.size(),CV_8UC1);
        mask(r)=255;

        // 计算修复半径（取矩形短边5%，范围限制在3~15像素）
        int radius=std::max(3,std::min(15,static_cast<int>(std::min(r.width,r.height)*0.05)));

        // 使用Telea算法进行图像修复
        cv::Mat result;
        cv::inpaint(src,mask,result,radius,cv::INPAINT_TELEA);
        return result;
    }


    static void _prepareMask(const cv::Mat& src, cv::Mat& mask, const cv::Rect& rect)
    {
        /*
         * @brief 准备掩膜图像（供外部调用）
         * @param src 输入原图像
         * @param mask 输出掩膜图像
         * @param rect 水印区域矩形
         */
        // 参数校验
        if(src.empty()||rect.width<=0||rect.height<=0)
        {
            mask=cv::Mat::zeros(src.size(),CV_8UC1);
            return;
        }

        // 边界裁剪
        cv::Rect r=rect;
        r.x=std::max(0,r.x);
        r.y=std::max(0,r.y);
        r.width=std::min(r.width,src.cols-r.x);
        r.height=std::min(r.height,src.rows-r.y);

        // 生成掩膜
        mask=cv::Mat::zeros(src.size(),CV_8UC1);
        if(r.width>0&&r.height>0)mask(r)=255;
    }

    bool ImageTools::removeWatermark(
            const string &imgPath,
            const string &outputPath,
            int topX, int topY,
            int btnX,int btnY
    )
    {
        /*
         * @brief 去除水印并保存结果到文件
         * @param imgPath 输入图像路径
         * @param outputPath 输出图像路径
         * @param topX 水印区域左上角X坐标
         * @param topY 水印区域左上角Y坐标
         * @param btnX 水印区域右下角X坐标
         * @param btnY 水印区域右下角Y坐标
         * @return true表示成功，false表示失败
         */

        // 编码转换（支持中文路径） //
        // 读取图像
        cv::Mat src=_imreadUnicode(imgPath,cv::IMREAD_COLOR);
        if(src.empty())return false;

        // 由两点坐标构建矩形（左上→右下）
        cv::Rect rect(topX,topY,btnX-topX,btnY-topY);

        // 执行去水印
        cv::Mat result=_removeWatermarkByRect(src,rect);
        if(result.empty())return false;

        // 保存结果
        return _imwriteUnicode(outputPath, result);
    }


    std::vector<unsigned char>ImageTools::removeWatermarkBin(
            const string &imgPath,
            int topX, int topY,
            int btnX,int btnY
    )
    {
        /*
         * @brief 去除水印并返回二进制数据（内存操作版本）
         * @param imgPath 输入图像路径
         * @param outputPath 输出图像路径（实际未使用，保留接口兼容性）
         * @param topX 水印区域左上角X坐标
         * @param topY 水印区域左上角Y坐标
         * @param btnX 水印区域右下角X坐标
         * @param btnY 水印区域右下角Y坐标
         * @return PNG格式的图像二进制数据，失败返回空vector
         */

        // 编码转换

        // 读取图像
        cv::Mat src=_imreadUnicode(imgPath,cv::IMREAD_COLOR);
        if(src.empty())return{};

        // 构建矩形并去水印
        cv::Rect rect(topX,topY,btnX-topX,btnY-topY);
        cv::Mat result=_removeWatermarkByRect(src,rect);
        if(result.empty())return{};

        // 编码为PNG格式的字节流
        std::vector<unsigned char>buf;
        if(!cv::imencode(".png",result,buf))return{};
        return buf;
    }


    // ======================= 带手动缩放的匹配（...Zoom 系列） ======================= //
    // 说明：这些函数是新增的重载入口，手动传入模板采集时电脑的缩放（1.5=150%）。
    // 上面不带 Zoom 的同名函数及其内部实现保持不变，仍为"同机同缩放"按原尺寸匹配。

    ImagePosition ImageTools::getImagePositionZoom(const std::string &filePath, double threshold,
                                                   double templateZoom, bool notException)
    {
        ImagePosition position;
        position.result = false;
        position.x = -1;
        position.y = -1;

        SetProcessDPIAware();

        cv::Point origin;
        cv::Mat screenshot = _captureScreen(&origin);

        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            if (!notException)
                wtl::ThrowError::showError("The template image cannot be loaded!");
            return position;
        }

        ScaledMatch m = _matchTemplateAtZoom(screenshot, templateImg, templateZoom);
        if (m.maxVal >= threshold)
        {
            position.x = origin.x + m.maxLoc.x + m.matchedSize.width / 2;
            position.y = origin.y + m.maxLoc.y + m.matchedSize.height / 2;
            position.result = true;
        }
        else if (!notException)
        {
            wtl::ThrowError::showError("Image not found!");
        }
        return position;
    }

    std::vector<ImagePosition> ImageTools::getImagePositionVecZoom(const std::string &filePath, double threshold,
                                                                   double templateZoom, bool notException, int maxMatches)
    {
        std::vector<ImagePosition> positions;

        SetProcessDPIAware();

        cv::Point origin;
        cv::Mat screenshot = _captureScreen(&origin);

        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            if (!notException)
                wtl::ThrowError::showError("The template image cannot be loaded!");
            return positions;
        }

        cv::Mat grayScreen = _toGray(screenshot);
        cv::Mat grayTpl = _toGray(templateImg);

        // 手动缩放：按 本机缩放 / templateZoom 归一到本机物理像素。
        double base = (templateZoom > 0.0)
                      ? static_cast<double>(_getMediaScreenZoom()) / templateZoom
                      : 1.0;

        struct RawMatch { double val; cv::Point center; };
        std::vector<RawMatch> raw;

        cv::Mat tpl;
        cv::resize(grayTpl, tpl, cv::Size(), base, base, cv::INTER_LINEAR);
        if (!tpl.empty() && tpl.rows <= grayScreen.rows && tpl.cols <= grayScreen.cols)
        {
            cv::Mat result;
            cv::matchTemplate(grayScreen, tpl, result, cv::TM_CCOEFF_NORMED);

            while (true)
            {
                double minVal, maxVal;
                cv::Point minLoc, maxLoc;
                cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
                if (maxVal < threshold)
                    break;

                raw.push_back({maxVal, cv::Point(origin.x + maxLoc.x + tpl.cols / 2,
                                                 origin.y + maxLoc.y + tpl.rows / 2)});

                int maskRadius = tpl.cols / 2;
                cv::Rect maskRect(
                        std::max(0, maxLoc.x - maskRadius),
                        std::max(0, maxLoc.y - maskRadius),
                        std::min(tpl.cols + maskRadius * 2, result.cols - std::max(0, maxLoc.x - maskRadius)),
                        std::min(tpl.rows + maskRadius * 2, result.rows - std::max(0, maxLoc.y - maskRadius))
                );
                cv::Mat roi = result(maskRect);
                roi.setTo(-1);
            }
        }

        std::sort(raw.begin(), raw.end(), [](const RawMatch& a, const RawMatch& b){ return a.val > b.val; });

        if (maxMatches <= 0)
            maxMatches = INT_MAX;

        int dedupRadius = grayTpl.cols / 2;
        for (const auto& r : raw)
        {
            bool dup = false;
            for (const auto& p : positions)
            {
                int dx = p.x - r.center.x;
                int dy = p.y - r.center.y;
                if (dx * dx + dy * dy < dedupRadius * dedupRadius)
                {
                    dup = true;
                    break;
                }
            }
            if (dup)
                continue;

            ImagePosition position;
            position.x = r.center.x;
            position.y = r.center.y;
            position.result = true;
            positions.push_back(position);

            if ((int)positions.size() >= maxMatches)
                break;
        }

        if (positions.empty() && !notException)
            wtl::ThrowError::showError("Image not found!");

        return positions;
    }

    ImagePosition ImageTools::getSpecifiedAreaImagePositionZoom(int x1, int y1, int x2, int y2,
                                                                const string& filePath, double threshold,
                                                                double templateZoom, bool notException)
    {
        ImagePosition position;
        position.result = false;
        position.x = -1;
        position.y = -1;

        SetProcessDPIAware();

        int width = x2 - x1;
        int height = y2 - y1;
        if (width <= 0 || height <= 0)
        {
            if (!notException)
                wtl::ThrowError::showError("Invalid specified area coordinates!");
            return position;
        }

        cv::Mat screenshot = _captureScreenArea(x1, y1, width, height);

        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            if (!notException)
                wtl::ThrowError::showError("The template image cannot be loaded!");
            return position;
        }

        ScaledMatch m = _matchTemplateAtZoom(screenshot, templateImg, templateZoom);
        if (m.maxVal >= threshold)
        {
            position.x = x1 + m.maxLoc.x + m.matchedSize.width / 2;
            position.y = y1 + m.maxLoc.y + m.matchedSize.height / 2;
            position.result = true;
        }
        else if (!notException)
        {
            wtl::ThrowError::showError("Image not found in the specified area!");
        }
        return position;
    }

    bool ImageTools::findSimilarImageZoom(const string& filePath, double threshold, double templateZoom)
    {
        SetProcessDPIAware();

        cv::Mat screenshot = _captureScreen();
        if (screenshot.empty())
            return false;

        cv::Mat templateImg = _imreadUnicode(filePath, cv::IMREAD_COLOR);
        if (templateImg.empty())
            return false;

        ScaledMatch m = _matchTemplateAtZoom(screenshot, templateImg, templateZoom);
        return (m.maxVal >= threshold);
    }

    bool ImageTools::findSpecifiedSimilarImageZoom(int x1, int y1, int x2, int y2, const std::string &filePath,
                                                   double threshold, double templateZoom)
    {
        SetProcessDPIAware();

        int width = x2 - x1;
        int height = y2 - y1;
        if (width <= 0 || height <= 0)
            return false;

        cv::Mat screenshot = _captureScreenArea(x1, y1, width, height);
        if (screenshot.empty())
            return false;

        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
            return false;

        ScaledMatch m = _matchTemplateAtZoom(screenshot, templateImg, templateZoom);
        return (m.maxVal >= threshold);
    }

    ImageRect ImageTools::getImageRectZoom(const string& filePath, double threshold, double templateZoom, bool notException)
    {
        ImageRect rect;
        rect.result = false;
        rect.topX = -1;
        rect.topY = -1;
        rect.btnX = -1;
        rect.btnY = -1;

        SetProcessDPIAware();

        cv::Point origin;
        cv::Mat screenshot = _captureScreen(&origin);

        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            if (!notException)
                wtl::ThrowError::showError("The template image cannot be loaded!");
            return rect;
        }

        ScaledMatch m = _matchTemplateAtZoom(screenshot, templateImg, templateZoom);
        if (m.maxVal >= threshold)
        {
            rect.topX = origin.x + m.maxLoc.x;
            rect.topY = origin.y + m.maxLoc.y;
            rect.btnX = origin.x + m.maxLoc.x + m.matchedSize.width;
            rect.btnY = origin.y + m.maxLoc.y + m.matchedSize.height;
            rect.result = true;
        }
        else if (!notException)
        {
            wtl::ThrowError::showError("Image not found!");
        }
        return rect;
    }

    // ======================= 便捷重载（缩放作为第 3 个参数） ======================= //
    // 统一转发到 ...Zoom 实现，避免把缩放误当成 notException（bool）静默吞掉。

    ImagePosition ImageTools::getImagePosition(const std::string &filePath, double threshold,
                                               double templateZoom, bool notException)
    {
        return getImagePositionZoom(filePath, threshold, templateZoom, notException);
    }

    std::vector<ImagePosition> ImageTools::getImagePositionVec(const std::string &filePath, double threshold,
                                                               double templateZoom, bool notException, int maxMatches)
    {
        return getImagePositionVecZoom(filePath, threshold, templateZoom, notException, maxMatches);
    }

    ImagePosition ImageTools::getSpecifiedAreaImagePosition(int x1, int y1, int x2, int y2,
                                                            const string& filePath, double threshold,
                                                            double templateZoom, bool notException)
    {
        return getSpecifiedAreaImagePositionZoom(x1, y1, x2, y2, filePath, threshold, templateZoom, notException);
    }

    bool ImageTools::findSimilarImage(const string& filePath, double threshold, double templateZoom)
    {
        return findSimilarImageZoom(filePath, threshold, templateZoom);
    }

    bool ImageTools::findSpecifiedSimilarImage(int x1, int y1, int x2, int y2, const std::string &filePath,
                                               double threshold, double templateZoom)
    {
        return findSpecifiedSimilarImageZoom(x1, y1, x2, y2, filePath, threshold, templateZoom);
    }

    ImageRect ImageTools::getImageRect(const string& filePath, double threshold, double templateZoom, bool notException)
    {
        return getImageRectZoom(filePath, threshold, templateZoom, notException);
    }

    ImageTools::~ImageTools()
    {
        if (isCaptureScreen)
        {
            // 释放 COM 资源
            CoUninitialize();
        }
    }


        /************************* 视频时间 ****************************/
        VideoTools::VideoTools()
        {
            ;
        }

    // FFMPEG版  //
    // 剪切视频 //
        bool VideoTools::videoArrivalTargetStop(const std::string& input_path, const std::string& output_path,
                                                double start_sec, double stop_sec, bool useFFMPEG, bool useGpu, bool show_errors)
        {
            if (useFFMPEG)
            {
                // FFMPEG版本

                // 计算持续时间
                double duration = stop_sec - start_sec;

                // 构建基础命令
                std::string cmd = "ffmpeg -loglevel " + std::string(show_errors ? "error" : "quiet") +
                                  " -ss " + std::to_string(start_sec) +
                                  " -i \"" + input_path + "\" -t " +
                                  std::to_string(duration) + " -avoid_negative_ts 1 ";

                // 添加GPU加速判断
                if (useGpu) {
                    // 使用GPU硬件编码（这里以NVIDIA NVENC为例）
                    cmd += "-c:v h264_nvenc -preset fast ";
                } else {
                    // 使用CPU复制流
                    cmd += "-c copy ";
                }

                // 添加输出路径和覆盖选项
                cmd += "\"" + output_path + "\" -y";

                int result = _wsystem(fs::path(cmd).wstring().c_str());

                if (result != 0 && show_errors) {
                    std::cerr << "FFmpeg command execution failed: " << cmd << std::endl;
                }

                return result == 0;
            }
            else
            {
                // OpenCV版本（保持原有逻辑，路径保持 UTF-8）
                std::string inputPath = input_path;
                std::string outputPath = output_path;

                // 第1步: 打开输入视频
                cv::VideoCapture cap(inputPath);
                if (!cap.isOpened()) {
                    std::cerr << "Error: Unable to open input video: " << inputPath << std::endl;
                    return false;
                }

                // 获取视频属性
                double fps = cap.get(cv::CAP_PROP_FPS);
                int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
                int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
                int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

                if (fps <= 0) {
                    std::cerr << "Error: Unable to get video frame rate." << std::endl;
                    return false;
                }

                // 计算视频总时长（秒）
                double video_duration = total_frames / fps;

                // 检查截断长度是否大于视频长度
                if (start_sec > video_duration) {
                    std::cerr << "Error: Start time exceeds video duration. Video duration: "
                              << video_duration << " seconds, requested start: " << start_sec << " seconds." << std::endl;
                    return false;
                }

                if (stop_sec > video_duration) {
                    std::cerr << "Error: Stop time exceeds video duration. Video duration: "
                              << video_duration << " seconds, requested stop: " << stop_sec << " seconds." << std::endl;
                    return false;
                }

                // 计算开始和结束帧索引
                int start_frame = static_cast<int>(start_sec * fps);
                int stop_frame = static_cast<int>(stop_sec * fps);

                // 确保开始和结束帧在有效范围内
                if (start_frame < 0) start_frame = 0;
                if (stop_frame > total_frames) stop_frame = total_frames;
                if (start_frame >= stop_frame) {
                    std::cerr << "Error: Start time should be less than stop time, and stop frame should be greater than start frame." << std::endl;
                    return false;
                }

                // 跳转到开始帧
                cap.set(cv::CAP_PROP_POS_FRAMES, start_frame);

                // 第2步: 创建VideoWriter以保存输出视频
                int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v'); // MP4编码
                cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(width, height));
                if (!writer.isOpened()) {
                    std::cerr << "Error: Unable to create output video file: " << outputPath << std::endl;
                    return false;
                }

                // 第3步: 逐帧读取并写入，直到结束帧
                cv::Mat frame;
                int current_frame = start_frame;
                while (current_frame <= stop_frame) {
                    if (!cap.read(frame)) {
                        std::cerr << "Warning: Read failed at frame " << current_frame << ", terminating early." << std::endl;
                        break;
                    }
                    writer.write(frame);
                    current_frame++;
                }

                // 第4步: 释放资源
                cap.release();
                writer.release();

                return true;
            }
        }

        static bool _compositeVideoCv(const std::vector<std::string>& inputPaths,
                                      const std::string& outputPath,
                                      double fps = 0.0)
        {
            // 路径保持 UTF-8（OpenCV 的 VideoCapture/VideoWriter 只接受 std::string）
            std::string output_path = outputPath;
            const std::vector<std::string>& converted_inputs = inputPaths;

            if (inputPaths.empty()) {
                return false;
            }

            std::vector<cv::VideoCapture> captures;

            // 1. 打开所有输入视频
            for (const auto& path : converted_inputs) {
                cv::VideoCapture cap(path);
                if (!cap.isOpened()) {
                    return false;
                }
                captures.push_back(cap);
            }

            // 2. 获取第一个视频的参数作为基准
            int width = 0;
            int height = 0;
            double input_fps = 0.0;
            int fourcc = 0;

            // 获取第一个视频的参数
            cv::Mat firstFrame;
            captures[0] >> firstFrame;
            if (firstFrame.empty()) {
                return false;
            }

            // 重置第一个视频的位置，因为我们消耗了一帧
            captures[0].set(cv::CAP_PROP_POS_FRAMES, 0);

            width = firstFrame.cols;
            height = firstFrame.rows;
            input_fps = captures[0].get(cv::CAP_PROP_FPS);
            fourcc = static_cast<int>(captures[0].get(cv::CAP_PROP_FOURCC));

            // 获取视频的总帧数（可选，用于进度显示）
            std::vector<int> totalFrames;
            for (auto& cap : captures) {
                int frameCount = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
                totalFrames.push_back(frameCount);
            }

            // 3. 设置输出视频参数
            double output_fps = (fps > 0.0) ? fps : input_fps;
            if (output_fps <= 0) output_fps = 30.0;

            int output_width = width;  // 不进行水平拼接，只是顺序连接
            int output_height = height;

            // 4. 创建视频写入器
            cv::VideoWriter writer;
            if (fourcc == 0) {
                fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
            }

            writer.open(output_path, fourcc, output_fps, cv::Size(output_width, output_height), true);
            if (!writer.isOpened()) {
                return false;
            }

            // 5. 顺序处理所有视频
            for (size_t videoIdx = 0; videoIdx < captures.size(); ++videoIdx) {
                cv::VideoCapture& cap = captures[videoIdx];

                // 重置当前视频到开始位置
                cap.set(cv::CAP_PROP_POS_FRAMES, 0);

                while (true) {
                    cv::Mat frame;
                    cap >> frame;

                    if (frame.empty()) {
                        // 当前视频已结束，跳出循环处理下一个视频
                        break;
                    }

                    // 将当前帧写入输出视频
                    writer.write(frame);
                }

                // 重置捕获对象，准备处理下一个视频
                cap.release();
            }

            // 6. 释放资源
            writer.release();
            for (auto& cap : captures) {
                if (cap.isOpened()) {
                    cap.release();
                }
            }

            return true;
        }

    //////// 辅助部分 ////////
        static void _splitAudioTracks(const std::vector<std::string>& videoPath, const string& outputPath)
        {
            AudioTools msc;
            string mp3_dir = wtl::format("{}/{}", outputPath, "temp_mp3");

            // 创建临时音频目录
            fs::create_directories(mp3_dir);

            // 存储生成的音频文件路径
            std::vector<std::string> audioPaths;

            for (size_t idx = 0; idx < videoPath.size(); ++idx)
            {
                // 生成唯一的音频文件名
                string audioFile = wtl::format("{}/audio_{:03d}.mp3", mp3_dir, idx);

                // 拆分音频
                if (msc.splitAudioTracks(videoPath[idx], audioFile))
                {
                    audioPaths.push_back(audioFile);
                }
                else
                {
                    // 如果拆分失败，记录错误
                    wtl::println(wtl::Color("red"), "Audio splitting failed:", videoPath[idx]);
                }
            }

            if (!audioPaths.empty())
            {
                // 拼接所有音频文件
                string finalAudio = wtl::format("{}/final_audio.mp3", outputPath);
                bool compositeResult = msc.compositeMusic(audioPaths, finalAudio);

                if (compositeResult)
                {
                    wtl::println(wtl::Color("green"), "Audio stitching completed:", finalAudio);
                }
                else
                {
                    wtl::println(wtl::Color("red"), "Audio concatenation failed");
                }
            }
            else
            {
                wtl::println(wtl::Color("yellow"), "There are no audio files available for concatenation.");
            }

        }

    // 辅助函数：获取视频编码格式
        std::string VideoTools::getVideoCodec(const std::string& videoPath)
        {
            // 构建 ffprobe 命令获取编码信息
            std::string cmd = "ffprobe -v error -select_streams v:0 "
                              "-show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 \"";
            cmd += videoPath + "\"";

            std::array<char, 128> buffer;
            std::string result;
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (!pipe) {
                return "unknown";
            }

            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
            }
            _pclose(pipe);

            // 清理结果字符串
            result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
            result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());

            return result.empty() ? "unknown" : result;
        }


        static string _changeToTsFile(const std::vector<string> &inputPaths, const string &video_temp_dir)
        {
            VideoTools vde;
            // 视频转化为ts格式
            std::vector<std::string> tsFiles;

            for (size_t i = 0; i < inputPaths.size(); ++i) {
                if (!fs::exists(inputPaths[i])) {
                    // 文件不存在，跳过
                    continue;
                }

                // 使用索引作为文件名，避免特殊字符问题
                std::string tsFileName = video_temp_dir + "/video_" + std::to_string(i) + ".ts";

                // 1. 获取视频编码格式 //
                std::string codec = vde.getVideoCodec(inputPaths[i]);
                std::string ffmpeg_cmd = "";

                // 2. 根据编码格式构建不同的命令
                if (codec == "h264" || codec == "avc" || codec == "avc1") {
                    // H.264 编码，使用 h264_mp4toannexb
                    ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                                 "-bsf:v h264_mp4toannexb -f mpegts -y \"" + tsFileName + "\"";
                }
                else if (codec == "hevc" || codec == "h265") {
                    // HEVC/H.265 编码，尝试使用 hevc_mp4toannexb
                    ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                                 "-bsf:v hevc_mp4toannexb -f mpegts -y \"" + tsFileName + "\"";
                }
                else if (codec == "mpeg4" || codec == "mpeg2video" || codec == "mpeg1video") {
                    // MPEG 系列编码，不使用比特流过滤器
                    ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                                 "-f mpegts -y \"" + tsFileName + "\"";
                }
                else {
                    // 其他未知编码，先尝试不使用过滤器
                    ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                                 "-f mpegts -y \"" + tsFileName + "\"";
                }

                // 3. 执行转换命令
                fs::path cmd = ffmpeg_cmd;
                int ret = _wsystem(cmd.wstring().c_str());

                // 4. 如果转换失败，尝试备用方案
                if (ret != 0) {
                    // 方案A：对于 HEVC 编码，尝试不使用比特流过滤器
                    if (codec == "hevc" || codec == "h265") {
                        ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                                     "-f mpegts -y \"" + tsFileName + "\"";
                        cmd = ffmpeg_cmd;
                        ret = _wsystem(cmd.wstring().c_str());
                    }
                    // 方案B：如果仍然失败，尝试重新编码（慢但兼容性最好）
                    if (ret != 0) {
                        ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" " +
                                     "-c:v libx264 -preset medium -c:a aac -f mpegts -y \"" + tsFileName + "\"";
                        cmd = ffmpeg_cmd;
                        ret = _wsystem(cmd.wstring().c_str());
                    }

                    if (ret != 0) {
                        // 转换失败，清理临时文件并返回
                        for (const auto& file : tsFiles) {
                            fs::remove(file);
                        }
                        return "";
                    }
                }

                tsFiles.push_back(tsFileName);
            }

            if (tsFiles.empty()) return "";

            // 5. 构建 concat 字符串
            std::stringstream concatStream;
            concatStream << "concat:";
            for (size_t i = 0; i < tsFiles.size(); ++i) {
                concatStream << tsFiles[i];
                if (i != tsFiles.size() - 1) {
                    concatStream << "|";
                }
            }
            std::string concatStr = concatStream.str();
            return concatStr;
        }

        // 合并视频 //
        bool VideoTools::compositeVideo(const std::vector<std::string>& inputPaths,
                                        const std::string& outputPath,
                                        double fps,bool useGpu)
        {
            AudioTools msc;
            // 临时音频视频路径
            vector<string> videoTempList;
            vector<string> musicTempList;

            /***** 生成临时文件目录 *****/
            string nowTime = _getMediaNowTime();
    //    auto re_find_file_name = re::findAll(R"(\w+-\w+-\w+)",nowTime);
            auto re_find_file_name = re::sub("[-: ]","_",nowTime);
            string temp_dir_name = wtl::format("./temp_{}", re_find_file_name);
            fs::create_directories(temp_dir_name);

            // 开启线程拆分音频
            std::thread T1(_splitAudioTracks, inputPaths, temp_dir_name);
            T1.join();

            // 创建视频临时目录
            string video_temp_dir = wtl::format("{}/{}", temp_dir_name, "temp_video_ts");
            fs::create_directories(video_temp_dir);
            tempPathList.push_back(video_temp_dir);

    #ifdef DEBUG
            println("compositeVideo函数: 继续执行!");
    #endif

            // 转为Ts文件
            string allCmdToTsVideo = _changeToTsFile(inputPaths, video_temp_dir);

            // 4. 合并TS文件
            string save_ts_path = video_temp_dir + "/temp.mp4";
            std::string mergeCmd = "ffmpeg -loglevel error -i \"" + allCmdToTsVideo + "\" -c copy " +
                                   "-bsf:a aac_adtstoasc -y \"" + save_ts_path + "\"";

            // 如果需要指定帧率
            if (fps > 0.0) {
                mergeCmd = "ffmpeg -loglevel error -i \"" + allCmdToTsVideo + "\" -c copy " +
                           "-r " + std::to_string(fps) + " " +
                           "-bsf:a aac_adtstoasc -y \"" + save_ts_path + "\"";
            }

            fs::path cmd_end = mergeCmd;
            bool result = _wsystem(cmd_end.wstring().c_str());

            // 如果处理失败返回false
            if (result != 0) {
                return false;
            }

            // 禁止视频的音频
            string end_temp_save = video_temp_dir + "/temp_end.mp4";
            bool end_save = removeVideoAudioTrack(save_ts_path, end_temp_save, useGpu);
            if (!end_save) {
                return false;
            }

            // 融合视频与音频
            // 注意：这里假设split_audio_tracks函数已经将音频合并为final_audio.mp3并保存在temp_dir_name目录下
            string final_audio_path = wtl::format("{}/final_audio.mp3", temp_dir_name);
            bool over = addAudioToVideo(end_temp_save, final_audio_path, outputPath, useGpu);
            wtl::FileManagement fl;
            fl.removeDir(temp_dir_name);
            return over;
        }

        bool VideoTools::setVideoCover(const std::string& videoPath,
                                       const std::string& imagePath,
                                       const std::string& outputPath,bool useGpu)
        {
            try {
                // 检查输入文件是否存在
                if (!fs::exists(videoPath)) {
                    std::cerr << "The video file does not exist.: " << videoPath << std::endl;
                    return false;
                }

                if (!fs::exists(imagePath)) {
                    std::cerr << "The cover image does not exist.: " << imagePath << std::endl;
                    return false;
                }

                // 创建临时目录
                fs::create_directories("./temp");
                tempPathList.emplace_back("./temp");

                // 生成临时文件名
                fs::path filePath = videoPath;
                std::string stem = filePath.stem().string();
                std::string tempVideoPath = "./temp/" + stem + "_temp.mp4";

                // 构建第一个FFmpeg命令（复制视频）
                std::string removeCmd = "ffmpeg -y -i \"" + videoPath + "\" ";

                if (useGpu) {
                    removeCmd += "-c:v h264_nvenc -preset fast -c:a copy ";
                } else {
                    removeCmd += "-c copy ";
                }

                removeCmd += "-map 0 \"" + tempVideoPath + "\" 2>nul";

                // 构建第二个FFmpeg命令（添加封面）
                std::string addCmd = "ffmpeg -y -i \"" + tempVideoPath +
                                     "\" -i \"" + imagePath +
                                     "\" -map 0 -map 1 ";

                if (useGpu) {
                    addCmd += "-c:v h264_nvenc -preset fast -c:a copy ";
                } else {
                    addCmd += "-c copy ";
                }

                addCmd += "-disposition:v:1 attached_pic \"" + outputPath + "\" 2>nul";

                // 执行命令
                int ret1 = _wsystem(fs::path(removeCmd).wstring().c_str());
                int ret2 = _wsystem(fs::path(addCmd).wstring().c_str());

                // 清理临时文件
                fs::remove(tempVideoPath);
                fs::remove("./temp");

                return (ret1 == 0 && ret2 == 0);

            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << std::endl;
                return false;
            }
        }

        long long VideoTools::getVideoDuration(const std::string& videoPath)
        {
            // 路径保持 UTF-8
            std::string video_path = videoPath;

            // 1. 打开视频文件
            cv::VideoCapture cap(video_path);

            // 检查是否成功打开
            if (!cap.isOpened()) {
                std::cerr << "Error: Could not open the video file: " << video_path << std::endl;
                return -1;
            }

            // 2. 获取关键属性
            // 总帧数
            long long totalFrameCount = static_cast<long long>(cap.get(cv::CAP_PROP_FRAME_COUNT));
            // 帧率 (Frames Per Second)
            double fps = cap.get(cv::CAP_PROP_FPS);

            // 检查获取的值是否有效
            if (fps <= 0.0 || totalFrameCount <= 0) {
                // 尝试备选方案：通过读取直到结束来获取
                fps = cap.get(cv::CAP_PROP_FPS);
                if (fps <= 0.0) {
                    // 使用默认帧率
                    fps = 30.0;
                }

                // 如果总帧数不可用，尝试计算
                if (totalFrameCount <= 0) {
                    // 回退方案：手动计算
                    cap.set(cv::CAP_PROP_POS_AVI_RATIO, 1.0);
                    double totalTime = cap.get(cv::CAP_PROP_POS_MSEC) / 1000.0; // 秒
                    cap.set(cv::CAP_PROP_POS_AVI_RATIO, 0.0);

                    if (totalTime > 0) {
                        totalFrameCount = static_cast<long long>(totalTime * fps);
                    } else {
                        std::cerr << "Warning: Could not accurately determine video duration." << std::endl;
                        return -2; // 不同的错误码
                    }
                }
            }

            // 3. 计算时长（秒）
            double totalDurationSeconds = static_cast<double>(totalFrameCount) / fps;

            // 转换为微秒（1秒 = 1,000,000微秒）
            long long durationInMicroseconds = static_cast<long long>(totalDurationSeconds * 1000000.0 + 0.5); // 四舍五入

            // 4. 输出调试信息（可选）
    #ifdef DEBUG
            std::cout << "Video Information:" << std::endl;
        std::cout << "  Path: " << video_path << std::endl;
        std::cout << "  Total Frames: " << totalFrameCount << std::endl;
        std::cout << "  FPS: " << fps << std::endl;
        std::cout << "  Total Duration: " << totalDurationSeconds << " seconds" << std::endl;
        std::cout << "  Duration in microseconds: " << durationInMicroseconds << " µs" << std::endl;

        // 转换为可读格式
        int totalMinutes = static_cast<int>(totalDurationSeconds) / 60;
        int remainingSeconds = static_cast<int>(totalDurationSeconds) % 60;
        std::cout << "  Human Readable: " << totalMinutes << " minutes " << remainingSeconds << " seconds" << std::endl;
    #endif

            // 释放资源
            cap.release();

            return durationInMicroseconds;
        }

        double VideoTools::getVideoFrame(const std::string& videoPath)
        {
            // 路径保持 UTF-8
            std::string video_path = videoPath;

            VideoCapture cap(video_path);
            if (!cap.isOpened())
            {
                std::cerr << "Error: Could not open video file." << std::endl;
                return 0.0;
            }

            // 获取帧率（fps）
            double fps = cap.get(CAP_PROP_FPS);

            cap.release();
            return fps;
        }


    // 组合视频帧数 //
        bool VideoTools::imageFrameBinMergeVideo(
                std::vector<std::vector<unsigned char>>&video,
                const std::string &savePath,
                double fps
        ) noexcept
        {
            // 路径保持 UTF-8
            std::string save_path = savePath;

            if (video.empty())
            {
                cerr << "Error: Input video data is empty!" << endl;
                return false;
            }

            vector<Mat> frames;
            for (const auto& frameData : video)
            {
                if (frameData.empty())
                {
                    cerr << "Warning: Empty frame data, skip." << endl;
                    continue;
                }

                Mat img = imdecode(frameData, IMREAD_COLOR);
                if (img.empty())
                {
                    cerr << "Warning: Failed to decode frame, skip this frame." << endl;
                    continue;
                }
                frames.push_back(img);
            }

            if (frames.empty())
            {
                cerr << "Error: No valid frames decoded!" << endl;
                return false;
            }

            Size frameSize = frames[0].size();

            int fourcc = VideoWriter::fourcc('M', 'P', '4', 'V');
            // ======== 修改1：使用 fps 参数 ========
            VideoWriter writer(save_path, fourcc, fps, frameSize, true);

            if (!writer.isOpened()) {
                fourcc = VideoWriter::fourcc('X', 'V', 'I', 'D');
                // ======== 修改2：使用 fps 参数 ========
                writer.open(save_path, fourcc, fps, frameSize, true);
                if (!writer.isOpened()) {
                    cerr << "Error: Could not open video writer for path: " << save_path << endl;
                    return false;
                }
            }

            for (const auto& frame : frames)
            {
                writer.write(frame);
            }

            writer.release();
            return true;
        }

        bool VideoTools::removeVideoAudioTrack(const std::string &inputPath, const std::string &outputPath,bool useGpu)
        {
            std::string ffmpeg_cmd = "ffmpeg -hide_banner -loglevel error -y -i \"" + inputPath + "\" ";

            // 添加GPU加速判断
            if (useGpu) {
                // 使用GPU编码
                ffmpeg_cmd += "-c:v h264_nvenc -preset fast -an \"" + outputPath + "\"";
            } else {
                // 复制视频流
                ffmpeg_cmd += "-c:v copy -an \"" + outputPath + "\"";
            }

            return _wsystem(fs::path(ffmpeg_cmd).wstring().c_str()) == 0;
        }

        bool VideoTools::addAudioToVideo(const string &inputPath, const string &audioPath, const string &outputPath,bool useGpu, bool silent)
        {
            // 基础命令构建
            std::string ffmpeg_cmd = "ffmpeg ";

            // 添加日志级别
            if (silent) {
                ffmpeg_cmd += "-hide_banner -loglevel error ";
            } else {
                ffmpeg_cmd += "-loglevel error ";
            }

            // 添加输入文件
            ffmpeg_cmd += "-i \"" + inputPath + "\" ";
            ffmpeg_cmd += "-i \"" + audioPath + "\" ";

            // 添加GPU加速判断
            if (useGpu) {
                // 使用GPU编码视频流
                ffmpeg_cmd += "-c:v h264_nvenc -preset fast ";
            } else {
                // 使用默认编码
                ffmpeg_cmd += "-c copy ";
            }

            // 添加音频处理和输出
            ffmpeg_cmd += "-map 0:v:0 -map 1:a:0 ";
            ffmpeg_cmd += "-y \"" + outputPath + "\"";

            return _wsystem(fs::path(ffmpeg_cmd).wstring().c_str()) == 0;
        }

        VideoTools::~VideoTools()
        {
            wtl::FileManagement fl;
            for (auto &i:tempPathList)
            {
                try
                {
                    fl.removeDir(i);
                }
                catch (...) {}
            }
        }

        class AudioTools::Impl final : private IMFPMediaPlayerCallback
        {
        public:
            Impl() = default;

            ~Impl()
            {
                releasePlayer();
            }

            void setPath(const std::string& utf8Path)
            {
                std::string pathError;
                audioPath_ = _audioToWidePath(utf8Path, pathError);
                if (audioPath_.empty())
                {
                    setStoredError(
                            pathError.empty() ? "Audio path is empty" : pathError);
                }
                else
                {
                    clearError();
                }
            }

            std::string getLastError() const
            {
                std::lock_guard<std::mutex> lock(errorMutex_);
                return lastError_;
            }

            bool play()
            {
                if (!checkAudioPath("play"))
                {
                    return false;
                }

                const bool alreadyOpen = opened_;
                if (!open())
                {
                    return false;
                }

                if (alreadyOpen)
                {
                    if (playbackEnded_)
                    {
                        PROPVARIANT start;
                        PropVariantInit(&start);
                        start.vt = VT_UI8;
                        start.uhVal.QuadPart = 0;
                        const HRESULT result = mediaPlayer_->SetPosition(
                                MFP_POSITIONTYPE_100NS,
                                &start);
                        PropVariantClear(&start);
                        if (FAILED(result))
                        {
                            setHResultError(result, "Media player SetPosition");
                            return false;
                        }
                        playbackEnded_ = false;
                    }

                    mediaPlayer_->SetVolume(1.0f);
                    mediaPlayer_->SetMute(FALSE);
                    const HRESULT result = mediaPlayer_->Play();
                    if (FAILED(result))
                    {
                        setHResultError(result, "Media player Play");
                        return false;
                    }
                }

                clearError();
                return true;
            }

            bool pause()
            {
                if (!checkAudioPath("pause"))
                {
                    return false;
                }
                if (!opened_ || mediaPlayer_ == nullptr)
                {
                    writeError("Cannot pause audio: Audio is not open");
                    return false;
                }

                const HRESULT result = mediaPlayer_->Pause();
                if (FAILED(result))
                {
                    setHResultError(result, "Media player Pause");
                    return false;
                }

                clearError();
                return true;
            }

            bool stop()
            {
                if (!checkAudioPath("stop"))
                {
                    return false;
                }
                if (!opened_)
                {
                    writeError("Cannot stop audio: Audio is not open");
                    return false;
                }

                releasePlayer();
                clearError();
                return true;
            }

        private:
            bool checkAudioPath(const char* action)
            {
                if (!audioPath_.empty())
                {
                    return true;
                }

                std::string reason = getLastError();
                if (reason.empty() || reason.rfind("Cannot ", 0) == 0)
                {
                    reason = "Audio path is empty";
                }

                writeError(
                        std::string("Cannot ") + action + " audio: " + reason);
                return false;
            }

            bool open()
            {
                if (opened_)
                {
                    return true;
                }
                if (audioPath_.empty())
                {
                    writeError("Cannot play audio: Audio path is empty");
                    return false;
                }

                const DWORD attributes = GetFileAttributesW(audioPath_.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES)
                {
                    writeError(
                            "Cannot play audio: Audio file does not exist: "
                            + _audioToUtf8String(audioPath_));
                    return false;
                }
                if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    writeError("Cannot play audio: Audio path is a directory");
                    return false;
                }

                HRESULT result =
                        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                if (SUCCEEDED(result))
                {
                    comInitialized_ = true;
                }
                else if (result != RPC_E_CHANGED_MODE)
                {
                    setHResultError(result, "CoInitializeEx");
                    return false;
                }

                result = MFStartup(MF_VERSION);
                if (FAILED(result))
                {
                    setHResultError(result, "MFStartup");
                    releasePlayer();
                    return false;
                }
                mediaFoundationStarted_ = true;

                result = MFPCreateMediaPlayer(
                        nullptr,
                        FALSE,
                        MFP_OPTION_FREE_THREADED_CALLBACK,
                        this,
                        nullptr,
                        &mediaPlayer_);
                if (FAILED(result))
                {
                    setHResultError(result, "MFPCreateMediaPlayer");
                    releasePlayer();
                    return false;
                }

                std::string urlError;
                const std::wstring url =
                        _audioCreateFileUrl(audioPath_, urlError);
                if (url.empty())
                {
                    writeError(
                            urlError.empty()
                                    ? "Cannot play audio: Failed to create file URL"
                                    : "Cannot play audio: " + urlError);
                    releasePlayer();
                    return false;
                }

                result = mediaPlayer_->CreateMediaItemFromURL(
                        url.c_str(),
                        FALSE,
                        0,
                        nullptr);
                if (FAILED(result))
                {
                    setHResultError(result, "CreateMediaItemFromURL");
                    releasePlayer();
                    return false;
                }

                opened_ = true;
                playbackEnded_ = false;
                clearError();
                return true;
            }

            void releasePlayer()
            {
                if (mediaPlayer_ != nullptr)
                {
                    mediaPlayer_->Shutdown();
                    mediaPlayer_->Release();
                    mediaPlayer_ = nullptr;
                }

                if (mediaFoundationStarted_)
                {
                    MFShutdown();
                    mediaFoundationStarted_ = false;
                }

                if (comInitialized_)
                {
                    CoUninitialize();
                    comInitialized_ = false;
                }

                opened_ = false;
                playbackEnded_ = false;
            }

            void writeError(const std::string& error)
            {
                setStoredError(error);
                std::cerr << error << '\n';
            }

            void setStoredError(const std::string& error)
            {
                std::lock_guard<std::mutex> lock(errorMutex_);
                lastError_ = error;
            }

            void clearError()
            {
                setStoredError("");
            }

            void setHResultError(HRESULT result, const std::string& action)
            {
                wchar_t systemMessage[512] = {};
                FormatMessageW(
                        FORMAT_MESSAGE_FROM_SYSTEM
                                | FORMAT_MESSAGE_IGNORE_INSERTS,
                        nullptr,
                        static_cast<DWORD>(result),
                        0,
                        systemMessage,
                        static_cast<DWORD>(std::size(systemMessage)),
                        nullptr);

                std::wstring message =
                        std::wstring(action.begin(), action.end())
                        + L" failed: "
                        + std::to_wstring(result);
                if (systemMessage[0] != L'\0')
                {
                    message += L"\n";
                    message += systemMessage;
                }
                writeError(_audioToUtf8String(message));
            }

            HRESULT STDMETHODCALLTYPE QueryInterface(
                    REFIID interfaceId,
                    void** object) override
            {
                if (object == nullptr)
                {
                    return E_POINTER;
                }

                *object = nullptr;
                if (IsEqualIID(interfaceId, IID_IUnknown)
                    || IsEqualIID(
                            interfaceId,
                            IID_IMFPMediaPlayerCallback))
                {
                    *object = static_cast<IMFPMediaPlayerCallback*>(this);
                    AddRef();
                    return S_OK;
                }
                return E_NOINTERFACE;
            }

            ULONG STDMETHODCALLTYPE AddRef() override
            {
                return ++callbackReferences_;
            }

            ULONG STDMETHODCALLTYPE Release() override
            {
                return --callbackReferences_;
            }

            void STDMETHODCALLTYPE OnMediaPlayerEvent(
                    MFP_EVENT_HEADER* eventHeader) override
            {
                if (eventHeader == nullptr
                    || eventHeader->pMediaPlayer == nullptr)
                {
                    return;
                }

                if (eventHeader->eEventType
                    == MFP_EVENT_TYPE_MEDIAITEM_CREATED)
                {
                    if (FAILED(eventHeader->hrEvent))
                    {
                        setHResultError(
                                eventHeader->hrEvent,
                                "Create media item");
                        return;
                    }

                    MFP_MEDIAITEM_CREATED_EVENT* eventData =
                            MFP_GET_MEDIAITEM_CREATED_EVENT(eventHeader);
                    const HRESULT result =
                            eventHeader->pMediaPlayer->SetMediaItem(
                                    eventData->pMediaItem);
                    if (FAILED(result))
                    {
                        setHResultError(result, "SetMediaItem");
                    }
                    return;
                }

                if (eventHeader->eEventType == MFP_EVENT_TYPE_MEDIAITEM_SET)
                {
                    if (FAILED(eventHeader->hrEvent))
                    {
                        setHResultError(
                                eventHeader->hrEvent,
                                "Media item set");
                        return;
                    }

                    eventHeader->pMediaPlayer->SetVolume(1.0f);
                    eventHeader->pMediaPlayer->SetMute(FALSE);
                    const HRESULT result =
                            eventHeader->pMediaPlayer->Play();
                    if (FAILED(result))
                    {
                        setHResultError(result, "Media player Play");
                    }
                    return;
                }

                if (eventHeader->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED)
                {
                    playbackEnded_ = true;
                    return;
                }

                if (eventHeader->eEventType == MFP_EVENT_TYPE_ERROR)
                {
                    setHResultError(
                            eventHeader->hrEvent,
                            "Media player event");
                }
            }

            std::wstring audioPath_;
            IMFPMediaPlayer* mediaPlayer_ = nullptr;
            bool comInitialized_ = false;
            bool mediaFoundationStarted_ = false;
            bool opened_ = false;
            std::atomic<unsigned long> callbackReferences_{1};
            std::atomic<bool> playbackEnded_{false};
            mutable std::mutex errorMutex_;
            std::string lastError_ = "Audio path is empty";
        };

        AudioTools::AudioTools()
            : player_(std::make_unique<Impl>())
        {
        }

        AudioTools::AudioTools(const std::string& audioPath)
            : player_(std::make_unique<Impl>())
        {
            player_->setPath(audioPath);
        }

        AudioTools::~AudioTools() = default;
        AudioTools::AudioTools(AudioTools&&) noexcept = default;
        AudioTools& AudioTools::operator=(AudioTools&&) noexcept = default;

        bool AudioTools::playAudio()
        {
            return player_ != nullptr && player_->play();
        }

        bool AudioTools::pauseAudio()
        {
            return player_ != nullptr && player_->pause();
        }

        bool AudioTools::stopAudio()
        {
            return player_ != nullptr && player_->stop();
        }

        bool AudioTools::closeAudio()
        {
            return stopAudio();
        }

        std::string AudioTools::getLastError() const
        {
            return player_ == nullptr ? "" : player_->getLastError();
        }

        /************************* 音频操作 ****************************/
        bool AudioTools::compositeMusic(const std::vector<std::string>& inputPaths, const std::string& outputPath,bool useGpu)
        {
            if (inputPaths.empty() || outputPath.empty()) {
                return false;
            }

            // 方法2：使用concat滤镜但先统一格式
            std::stringstream cmd;
            cmd << "ffmpeg -v error -y ";

            // 添加所有输入文件
            for (const auto& input : inputPaths) {
                cmd << "-i \"" << input << "\" ";
            }

            // 为每个输入添加aformat滤镜统一格式
            cmd << "-filter_complex \"";
            for (size_t i = 0; i < inputPaths.size(); ++i) {
                cmd << "[" << i << ":a]aformat=sample_rates=44100:channel_layouts=stereo[a" << i << "]; ";
            }

            // 拼接统一格式后的音频
            for (size_t i = 0; i < inputPaths.size(); ++i) {
                cmd << "[a" << i << "]";
            }
            cmd << "concat=n=" << inputPaths.size() << ":v=0:a=1[out]\" ";

            // 音频编码设置
            cmd << "-map \"[out]\" -c:a libmp3lame -b:a 192k ";

            // 虽然音频处理GPU加速不明显，但保持参数一致性
            if (useGpu) {
                cmd << "-c:v h264_nvenc ";  // 如果有视频流的情况下
            }

            cmd << "\"" << outputPath << "\"";

            fs::path command = cmd.str();

    #ifdef DEBUG
            println(Color("yellow"), "ffmpeg命令:", command.string());
    #endif

            int result = _wsystem(command.wstring().c_str());
            return (result == 0);
        }

        bool AudioTools::splitAudioTracks(const std::string &videoPath, const std::string &audioSavePath,bool useGpu)
        {
            // 构建命令
            std::string cmd = "ffmpeg -hide_banner -loglevel quiet -y -i \"" + videoPath + "\" -vn ";

            // 音频编码设置
            cmd += "-acodec libmp3lame -q:a 4 ";

            // 虽然音频分离不太需要GPU加速，但保持参数一致性
            if (useGpu) {
                cmd += "-c:v h264_nvenc ";  // 如果有视频流的情况下
            }

            cmd += "\"" + audioSavePath + "\"";

            int result = _wsystem(fs::path(cmd).wstring().c_str());

            // system/_wsystem 调用成功返回0
            return (result == 0);
        }

        // 设置音频音量 //
        bool AudioTools::setSystemVolumeModern(int volumePercent)
        {
            if (volumePercent < 0) volumePercent = 0;
            if (volumePercent > 100) volumePercent = 100;

            HRESULT hr = S_OK;
            CoInitialize(NULL);

            // 创建设备枚举器
            IMMDeviceEnumerator* pEnumerator = NULL;
            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void**)&pEnumerator);
            if (FAILED(hr)) {
                CoUninitialize();
                return false;
            }

            // 获取默认音频端点
            IMMDevice* pDevice = NULL;
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (FAILED(hr)) {
                pEnumerator->Release();
                CoUninitialize();
                return false;
            }

            // 激活音量控制接口
            IAudioEndpointVolume* pEndpointVolume = NULL;
            hr = pDevice->Activate(__uuidof(IAudioEndpointVolume),
                                   CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
            if (FAILED(hr)) {
                pDevice->Release();
                pEnumerator->Release();
                CoUninitialize();
                return false;
            }

            // 设置音量（0.0到1.0之间）
            float volumeLevel = volumePercent / 100.0f;
            hr = pEndpointVolume->SetMasterVolumeLevelScalar(volumeLevel, NULL);

            // 静音（可选）
            // hr = pEndpointVolume->SetMute(TRUE, NULL);  // 静音
            // hr = pEndpointVolume->SetMute(FALSE, NULL); // 取消静音

            // 清理资源
            pEndpointVolume->Release();
            pDevice->Release();
            pEnumerator->Release();
            CoUninitialize();

            return SUCCEEDED(hr);
        }
}
