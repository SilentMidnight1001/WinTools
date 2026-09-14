#include "winToolsH.hpp"
#include <opencv2/opencv.hpp>
#include <gdiplus.h>

using namespace cv;
/******************** 获取图像坐标 ********************/

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
static cv::Mat _captureScreen()
{
    // 获取整个屏幕的设备上下文（DC）
    HDC hDC = GetDC(NULL);
    // 获取虚拟屏幕的尺寸（多显示器情况下）
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);  // 虚拟屏幕左上角 X 坐标
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);  // 虚拟屏幕左上角 Y 坐标

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

// =================== 多尺度模板匹配（DPI 归一化） =================== //
// templateZoom：模板采集时所在机器的缩放（如 1.0=100%、1.5=150%），由调用方手动传入。
// targetZoom：目标机器当前缩放，自动通过 getScreenZoom() 获取。
// 匹配时把模板按 targetZoom / templateZoom 缩放，并在此附近做 ±10% 小范围多尺度，
// 应对非整数缩放与字体渲染的 ±像素误差。
struct ScaledMatch
{
    double maxVal;          // 最佳匹配值
    cv::Point maxLoc;       // 最佳匹配左上角坐标（屏幕物理像素）
    cv::Size matchedSize;   // 命中时模板缩放后的尺寸
};

static ScaledMatch _matchTemplateScaled(const cv::Mat& screenshot, const cv::Mat& templateImg, double templateZoom)
{
    ScaledMatch best;
    best.maxVal = -1.0;
    best.maxLoc = cv::Point(0, 0);
    best.matchedSize = templateImg.size();

    float targetZoom = wtl::getScreenZoom();
    double baseScale = targetZoom / templateZoom;  // 模板从采集缩放 -> 目标物理像素
    double scales[] = {baseScale * 0.9, baseScale, baseScale * 1.1};

    for (double s : scales)
    {
        if (s <= 0.0)
            continue;
        cv::Mat tpl;
        cv::resize(templateImg, tpl, cv::Size(), s, s, cv::INTER_LINEAR);
        if (tpl.rows > screenshot.rows || tpl.cols > screenshot.cols)
            continue;  // 缩放后模板比屏幕大，跳过该尺度

        cv::Mat result;
        cv::matchTemplate(screenshot, tpl, result, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

        if (maxVal > best.maxVal)
        {
            best.maxVal = maxVal;
            best.maxLoc = maxLoc;
            best.matchedSize = tpl.size();
        }
    }
    return best;
}

// 获取图像坐标函数,将此函数移到图像操作中 //
ImagePosition wtl::ImageEvent::getImagePosition(const std::string &filePath, double threshold, double templateZoom,
                                           bool notException)
{
    if (!notException)
    {
        ImagePosition position;
        // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标
        SetProcessDPIAware();

        // 捕获屏幕截图
        cv::Mat screenshot = _captureScreen();

        // 加载模板图像（需确保 "img.png" 位于工作目录或提供绝对路径）
        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            wtl::ThrowError::showError("The template image cannot be loaded!");
        }

        // 多尺度模板匹配（自动适配当前机器 DPI 缩放）
        ScaledMatch m = _matchTemplateScaled(screenshot, templateImg, templateZoom);

        if (m.maxVal >= threshold)
        {
            // 计算目标中心坐标（使用命中尺度下的模板尺寸）
            position.x = m.maxLoc.x + m.matchedSize.width / 2;
            position.y = m.maxLoc.y + m.matchedSize.height / 2;
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
        cv::Mat screenshot = _captureScreen();

        // 加载模板图像（需确保 "img.png" 位于工作目录或提供绝对路径）
        cv::Mat templateImg = _imreadUnicode(filePath);
        if (templateImg.empty())
        {
            position.result = false;
            position.x = -1;
            position.y = -1;
            return position;
        }

        // 多尺度模板匹配（自动适配当前机器 DPI 缩放）
        ScaledMatch m = _matchTemplateScaled(screenshot, templateImg, templateZoom);

        if (m.maxVal >= threshold)
        {
            position.x = m.maxLoc.x + m.matchedSize.width / 2;
            position.y = m.maxLoc.y + m.matchedSize.height / 2;
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
std::vector<ImagePosition> wtl::ImageEvent::getImagePositionVec(const std::string &filePath, double threshold, double templateZoom,
                                                           bool notException, int maxMatches)
{
    std::vector<ImagePosition> positions;

    // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标
    SetProcessDPIAware();

    // 捕获屏幕截图
    cv::Mat screenshot = _captureScreen();

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
    std::vector<RawMatch> raw;

    float targetZoom = wtl::getScreenZoom();
    double baseScale = targetZoom / templateZoom;
    double scales[] = {baseScale * 0.9, baseScale, baseScale * 1.1};

    for (double s : scales)
    {
        cv::Mat tpl;
        cv::resize(templateImg, tpl, cv::Size(), s, s, cv::INTER_LINEAR);
        if (tpl.rows > screenshot.rows || tpl.cols > screenshot.cols)
            continue;

        cv::Mat result;
        cv::matchTemplate(screenshot, tpl, result, cv::TM_CCOEFF_NORMED);

        // 逐个找该尺度下所有 >= threshold 的匹配
        while (true)
        {
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
            if (maxVal < threshold)
                break;

            int cx = maxLoc.x + tpl.cols / 2;
            int cy = maxLoc.y + tpl.rows / 2;
            raw.push_back({maxVal, cv::Point(cx, cy)});

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
    }

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
ImagePosition wtl::ImageEvent::getSpecifiedAreaImagePosition(int x1, int y1, int x2, int y2,
                                                        const string& filePath, double threshold,double templateZoom,
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

        // 多尺度模板匹配（自动适配当前机器 DPI 缩放）
        ScaledMatch m = _matchTemplateScaled(screenshot, templateImg, templateZoom);

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

        // 多尺度模板匹配（自动适配当前机器 DPI 缩放）
        ScaledMatch m = _matchTemplateScaled(screenshot, templateImg, templateZoom);

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

bool wtl::ImageEvent::findSpecifiedSimilarImage(int x1, int y1, int x2, int y2, const std::string &filePath,
                                           double threshold, double templateZoom)
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

    // 多尺度模板匹配（自动适配当前机器 DPI 缩放）
    ScaledMatch m = _matchTemplateScaled(screenshot, templateImg, templateZoom);

    // 判断匹配结果是否满足阈值
    return (m.maxVal >= threshold);
}

bool wtl::ImageEvent::findSimilarImage(const string& filePath, double threshold, double templateZoom)
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

    // 多尺度模板匹配（自动适配当前机器 DPI 缩放）
    ScaledMatch m = _matchTemplateScaled(screenshot, templateImg, templateZoom);

    // 判断匹配结果是否达到阈值
    return (m.maxVal >= threshold);
}

bool wtl::ImageEvent::findSimilarImage(const string& templatePath, const string& comparePath, double threshold)
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
bool wtl::ImageEvent::setImageFormat(const string& imgPath, const string& targetImageFormat)
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
bool wtl::ImageEvent::_saveBitmapToFile(HBITMAP hBitmap, const wchar_t* filename)
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
bool wtl::ImageEvent::captureScreen(const std::string& imageName)
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

bool wtl::ImageEvent::captureScreen(const string& imageName, const string& targetImageSavePath)
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
string wtl::ImageEvent::_getImageNameFunc(const string& imgPath)
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

string wtl::ImageEvent::_getImageNameFunc(const string& imgPath, const string& name)
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
bool wtl::ImageEvent::setImageSize(const string& imgPath, float width, float height, const string& targetImageSavePath, const string& outPutName)
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

ImageSize wtl::ImageEvent::getImageSize(const string &filePath, bool outPutFlag)
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


bool wtl::ImageEvent::setImageSize(const string& imgPath, float width, float height)
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


bool wtl::ImageEvent::setImageSize(const string& imgPath, float width, float height, const string& outPutName)
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

ImageRect wtl::ImageEvent::getImageRect(const string& filePath, double threshold, double templateZoom, bool notException)
{
    ImageRect rect;
    rect.result = false;
    rect.topX = -1;
    rect.topY = -1;
    rect.btnX = -1;
    rect.btnY = -1;

    SetProcessDPIAware();

    // 捕获屏幕并加载模板
    cv::Mat screenshot = _captureScreen();
    cv::Mat templateImg = _imreadUnicode(filePath);
    if (templateImg.empty())
    {
        if (!notException)
        {
            wtl::ThrowError::showError("The template image cannot be loaded!");
        }
        return rect;
    }

    // 多尺度模板匹配（自动适配当前机器 DPI 缩放）
    ScaledMatch m = _matchTemplateScaled(screenshot, templateImg, templateZoom);

    if (m.maxVal >= threshold)
    {
        // 直接使用命中尺度下的模板尺寸计算矩形
        rect.topX = m.maxLoc.x;
        rect.topY = m.maxLoc.y;
        rect.btnX = m.maxLoc.x + m.matchedSize.width;
        rect.btnY = m.maxLoc.y + m.matchedSize.height;
        rect.result = true;
    }
    else if (!notException)
    {
        wtl::ThrowError::showError("Image not found!");
    }

    return rect;
}

// 模板图片从主图片中获取矩形位置 //
ImageRect wtl::ImageEvent::getImageRect(const string& templatePath, const string &filePath, double threshold, bool notException)
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
int wtl::ImageEvent::_getEncoderClsid(const WCHAR* format, CLSID* pClsid)
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

bool wtl::ImageEvent::drawRectOnImage(const std::string &imgPath,const std::string &outPutName,  int topX, int topY, int btnX, int btnY)
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
void wtl::ImageEvent::captureAreaScreen(const string& saveName, int x1, int y1, int x2, int y2)
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

bool wtl::ImageEvent::removeWatermark(
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


std::vector<unsigned char>wtl::ImageEvent::removeWatermarkBin(
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


wtl::ImageEvent::~ImageEvent()
{
    if (isCaptureScreen)
    {
        // 释放 COM 资源
        CoUninitialize();
    }
}
