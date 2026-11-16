#pragma once
#include <windows.h>
#include <iostream>
#include <wtlDict.hpp>
#include <wtlFileManagement.hpp>
#include <wtlMediaToolkit.hpp>
#include <wtlOcr.hpp>
#include <wtlStringCode.hpp>
#include <wtlTime.hpp>
#include <wtlOpen.hpp>
#include <wtlPrintFormat.hpp>
#include <wtlTypeName.hpp>
#include <wtlThrowError.hpp>
#include <wtlKeyBoard.hpp>
#include <wtlMouse.hpp>
#include <wtlZip.hpp>
#include <wtlRegex.hpp>
#include <ShellScalingApi.h>      // Windows DPI 缩放相关 API
#include <cstdio>
#include <ctime>
#include <csignal> // For signal()
#include <regex>  // 正则表达头文件
#include <vector>  // 自动管理内存
#include <thread>
#include <atomic>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <mmsystem.h>  // 需要包含这个头文件
#include <fstream>
#include <chrono>
#include <list>
#include <filesystem>
#include <map>
#include <system_error>
#include <tlhelp32.h>
#include <random>
#include <typeinfo>
#include <sstream>
#include <unordered_map>
#include <wincodec.h>  // WIC 头文件
#include <shellapi.h>
#include <numeric>
#include <tuple>
#include <mmdeviceapi.h>
#include <any>
#include <endpointvolume.h>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")

namespace
{
    namespace fs = std::filesystem;
    using std::string;
    using std::vector;
    using std::wstring;
    using std::ofstream;
    using std::ifstream;
    using std::cout;
    using std::cerr;
    using std::endl;
}
/****************** 调试器符号 ******************/
//#ifndef WtlDebug
//#define WtlDebug
//#endif

/****************** mingw专用 ******************/
#ifndef WtlMinGW
#define WtlMinGW
#endif

///****************** Vs编译器专用 ******************/
//#ifndef WtlVs
//#define WtlVs
//#endif


/***************** 定义宏 *****************/

// 改为正确的导出/导入模式：
#ifdef EXDLL
#define WTL __declspec(dllexport)
#else
#define WTL __declspec(dllimport)
#endif


// 定义一个检查是否注册末尾清理的变量 //
extern bool autoClear;

#ifdef WtlMinGW
/************* 屏幕事件 ****************/
typedef struct  // 声明结构体获取屏幕大小
{
    int x;
    int y;
    float screenZoom;
} ScreenSize;  // 定义标签

ScreenSize getScreenSize(bool out_put=true);  // 获取分辨率函数，布尔值选择是否输出坐标

#elif defined(WtlVs)
/************* 屏幕事件 ****************/
struct ScreenSize{
        int x;
        int y;
        float screenZoom;
    } ;  // 定义标签

ScreenSize getScreenSize(bool out_put=true);  // 获取分辨率函数，布尔值选择是否输出坐标

#endif

/****************** random *******************/
namespace random
{
    // 整数随机 //
    int randInt(int min, int max);
    long randInt(long min, long max);
    long long randInt(long long min, long long max);

    // 浮点数随机 //
    float uniform(float min, float max);
    double uniform(double min, double max);

#ifdef WtlMinGW
    // 随机选择列表容器 //
    template <class T>
    inline T choice(vector<T> vec)
    {
        auto randomNum = randInt(0, vec.size()-1);
        return vec[randomNum];
    }

    template <class T>
    inline vector<T> choices(const std::vector<T>& population, const std::vector<double>& weights, int k = 1) {
        // 参数检查
        if (population.empty() || k <= 0)
        {
            return {};
        }

        // 如果没有提供权重，则创建均匀权重
        std::vector<double> actualWeights;
        if (weights.empty())
        {
            actualWeights.assign(population.size(), 1.0);
        } else
        {
            // 检查权重数量是否与元素数量匹配
            if (weights.size() != population.size())
            {
                wtl::ThrowError::showError("Weights size must match population size");
            }
            actualWeights = weights;
        }

        // 计算前缀和（累积权重）
        std::vector<double> prefixSum(population.size());
        std::partial_sum(actualWeights.begin(), actualWeights.end(), prefixSum.begin());

        // 获取总权重
        double totalWeight = prefixSum.back();

        // 如果总权重为0或无效，返回空结果
        if (totalWeight <= 0)
        {
            return {};
        }

        // 准备随机数生成器
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(0.0, totalWeight);

        // 进行k次选择
        std::vector<T> result;
        result.reserve(k);

        for (int i = 0; i < k; ++i)
        {
            // 生成[0, totalWeight)之间的随机数
            double randomValue = dis(gen);

            // 使用二分查找找到第一个大于等于随机值的前缀和位置
            auto it = std::lower_bound(prefixSum.begin(), prefixSum.end(), randomValue);
            int index = std::distance(prefixSum.begin(), it);

            // 确保索引在有效范围内
            if (index >= 0 && index < population.size())
            {
                result.push_back(population[index]);
            }
        }

        return result;
    }

    // 简化版本：不需要权重的均匀选择 //
    template <class T>
    inline vector<T> choices(const std::vector<T>& population, int k = 1)
    {
        return choices(population, {}, k);
    }

#elif defined(WtlVs)
    // 随机选择列表容器 //
    template <class T>
    inline auto choice(vector<T> vec)
    {
        auto randomNum = randInt(0, vec.size()-1);
        return vec[randomNum];
    }

    // 简化版本：不需要权重的均匀选择 //
    template <class T>
    inline vector<T> choices(const std::vector<T>& population, int k = 1)
    {
        return choices(population, {}, k);
    }
#endif
}

/***************************** 文件对话框选择 ***************************/
/** Qt-style file/folder dialogs over WinAPI.
 *  fs::path handles UTF-8 ↔ wstring. StringCode for encoding detection.
 *  _filedialog helper namespace converts Qt filter → WinAPI lpstrFilter.
 */
namespace filedialog
{

/** Open single-file dialog (modern style via GetOpenFileName).
 *  @param caption  Dialog title (UTF-8, default: "选择文件")
 *  @param dir      Initial directory (UTF-8)
 *  @param filter   Qt-style: "Desc (*.ext);;Desc2 (*.ext2)"
 *  @return         Selected file path, or "" if cancelled
 */
    string askOpenFilename(
            const string &caption = "",
            const string &dir = "",
            const string &filter = "");

/** Open multi-file dialog.  Hold Ctrl/Shift to select multiple files.
 *  @param caption  Dialog title (UTF-8, default: "选择文件")
 *  @param dir      Initial directory (UTF-8)
 *  @param filter   Qt-style filter string
 *  @return         Vector of selected file paths (empty if cancelled)
 */
    vector<string> askOpenFilenames(
            const string &caption = "",
            const string &dir = "",
            const string &filter = "");

/** Open folder selection dialog (modern IFileOpenDialog style).
 *  @param caption  Dialog title (UTF-8, default: "选择文件夹")
 *  @param dir      Initial directory (UTF-8)
 *  @return         Selected folder path, or "" if cancelled
 */
    string askDirectory(
            const string &caption = "",
            const string &dir = "");

}

/******************************** 命名空间 ********************************/
namespace wtl
{
    // 时间模块兼容入口，新代码建议直接使用 wtl::time。
    // Compatibility entry points for the time module; new code should use wtl::time.
    // 返回自 Unix 纪元以来的秒数。
    // Returns seconds elapsed since the Unix epoch.
    inline double getRunTime()
    {
        return time::time();
    }

    // 返回当前本地时间字符串。
    // Returns the current local time string.
    inline string getNowTime()
    {
        return time::strftime("%Y-%m-%d %H:%M:%S", time::localtime());
    }

    // 暂停当前线程指定秒数。
    // Suspends the current thread for the specified number of seconds.
    inline void sleep(double delay)
    {
        time::sleep(delay);
    }

    /*************************** WinAPI 非类 **************************/
    // 检查管理员权限 | 以管理员权限启动 //
    // 检查已经使用管理员权限重新打开脚本 //
    bool isAdmin();
    bool openAdmin();

    // 开机自启 //
    bool setAutoStartUp(const fs::path &appName, const fs::path& exePath);
    bool setAutoStartUp(const fs::path &appName);
    bool setAutoStartUpCmd(const fs::path& appName, const fs::path& exePath);
    std::string getAutoStartExePath(const fs::path &appName);

    // 关闭开机自启 //
    bool disableAutoStart(const fs::path& appName);

    // 注入dll //
    bool loadToMemory(DWORD processId, const fs::path &dll_path);

    // 关闭控制台 //
    void hideConsole();
    // 打开控制台 //
    void showConsole();

    // 获取屏幕缩放比例 //
    float getScreenZoom();

    // 获取Windows rc文件 //
    std::vector<unsigned char> getWinRcData(int resourceId);


    std::string findExeInPath(const std::string& exeName,bool ignoreExe=false);

    // 防止重复打开exe //
    // 改进后的函数：通过进程名检查进程数量 //
    bool notRepeatOpenExe(const std::string& exeName);

    // 添加ico //
    bool setExeIcon(const std::filesystem::path& exePath, const std::filesystem::path& iconPath, const fs::path &saveFullPath="./");


    /***************** 进程杀死 *****************/
    bool killProcess(const fs::path &processName);

    // 增强版进程终止
    bool killProcess(const fs::path& processName,
                     bool killTree,
                     bool partialMatch);

    // 通过PID终止进程
    bool killProcess(DWORD pid, bool killTree = false);

    /*****************  *****************/

    // 获取控制台编码 utf8, gbk, big5 //
    string getConsoleEncoding();

    // 锁定窗口为焦点 //
    bool lockWindows(const std::string &windowName);
    bool lockWindows(int pid);

    // 自动设置 DPI 缩放 //
    void setAutoDpi();

    // 开启其他软件 //
    bool startOtherApp(const string& appPath);
    bool startOtherApp(const string& appPath, bool console);

    // 获取注册表键值 //
    string getRegistryPath(const std::string &keyPath, const std::string &valueName);

    /**
     * @brief 根据进程的可执行文件路径获取匹配的进程PID
     *
     * @param processPath 进程的可执行文件路径
     * @return DWORD 匹配的进程PID，如果未找到返回0
     */
    unsigned long getProcessPid(const fs::path& processPath);

    /**
     * @brief 根据进程的可执行文件路径获取所有匹配的进程PID
     *
     * @param processPath 进程的可执行文件路径
     * @return std::vector<DWORD> 匹配的进程PID列表
     */
    std::vector<DWORD> getProcessPid(const fs::path& processPath,bool getAllPidList);



    /*************************** 通用接口 非类 ***************************/
    // 运行时间 //


    // 获取当前exe名称 //
    string getNowExeName();

    // 字符串转容器,根据行数转化 //
    std::vector<std::string> stringToVector(const std::string& str);

    template<class T>
    bool findInVec(const std::vector<T>& vec, const T& target);

    // 冒泡排序法 //
    // up/down//
    vector<int>sort(const int *numList, int numListSize, const string &model);
    vector<int>sort(const std::vector<int> &numList, const string &model);

    // sum列表求和 //
    int sum(std::vector<int> &numList);
    long sum(std::vector<long> &numList);
    double sum(std::vector<double> &numList);
    float sum(std::vector<float> &numList);

    // 获取变量类型 //
    template<typename Types>
    string typeName(Types type);

    // 获取当前exe路径 //
    std::string getExePath(bool useWindows=false);

    // 加密函数，返回16进制字符串 //
    std::string addStringKey(const std::string& text, const std::string& key);

    // 解密函数，输入是16进制字符串 //
    std::string decryptStringKey(const std::string& hex_text, const std::string& key);

    // 嵌入二进制数据（按 flag 区块追加） //
    bool addBinaryResources(const string &exePath,const std::vector<unsigned char>& binary,const string &flag);

    // 解析指定 flag 的最后一个区块 //
    std::vector<unsigned char> parseEmbeddedBinaryData(const std::string &filePath,const string &flag);

    // 解析指定 flag 的所有区块 //
    std::vector<std::vector<unsigned char>> parseEmbeddedBinaryDataAll(const std::string &filePath,const string &flag);


    class SearchText
    {
    public:
        // 返回匹配的行号列表 //
        std::vector<long> findLines(const string& text, const string& pattern);
        long findLine(const string& text, const string& pattern);  // 返回第一个匹配行号，-1表示未找到

        // 返回匹配的行内容列表 //
        std::vector<string> findLinesContent(const string& text, const string& pattern);
        string findLineContent(const string& text, const string& pattern);  // 返回第一个匹配行内容

        // 返回 行号->内容 的映射 //
        wtl::Dict<int, string> findLinesMap(const string& text, const string& pattern);
        std::vector<wtl::Dict<int, string>> findLinesMapEach(const string& text, const string& pattern);
    };


        /************** Set UI TO Top Win ***************/
        class WindowTop
        {
        private:
            HWND hwnd; // 窗口句柄
            int px = 0;
            int py = 0;
            int w = 0;
            int h = 0;

            // 将窗口设置为顶层窗口
            void _setWindowTopMost(bool topMost);

            // 查找窗口句柄的辅助函数
            HWND _findWindowByTitle(const string& windowTitle);

            StringCode scd;

        public:
            // 构造函数，指定目标窗口句柄
            explicit WindowTop(const string& hwndName, int pxc=0, int pyc=0, int wc=0, int hc=0);
            explicit WindowTop(HWND hwnd, int pxc=0, int pyc=0, int wc=0, int hc=0);

            // 开启顶层窗口
            void setWindowsTopOn();

            // 关闭顶层窗口
            void setWindowsTopOff();
        };

        /*********************** 弹窗 ***************************/
        class WMessageBox
        {
        private:
            StringCode scd;

        public:
            WMessageBox();
            void showInfo(const fs::path &title, const fs::path &content);
            void showWarning(const fs::path &title, const fs::path &content);
            void showError(const fs::path &title, const fs::path &content);
            /* model: info warning error */
            bool askYesNo(const fs::path &title, const fs::path &content, const string model="info");
        };

    class CallWinDll
    {
    private:
        HMODULE hDll = nullptr;
        bool isOpenFlag = false;
        std::string callingConvention; // 存储调用约定的名称

    public:
        // 构造函数：增加第二个参数，默认值为"stdcall" //
        CallWinDll(const fs::path& winDllPath, const std::string& convention = "stdcall");
        ~CallWinDll();

        /**
         * @brief 检查当前对象是否处于打开状态
         * @return 返回一个布尔值，表示对象是否成功打开
         *         - true: 表示对象已成功打开
         *         - false: 表示对象未打开或打开失败
         */
        // 返回是否成功打开的信息 //
        inline bool isOpen() const { return isOpenFlag; }

        // 可选的：提供一个方法来修改已加载DLL的调用约定（如果DLL内函数约定一致） //
        inline void setCallingConvention(const std::string& convention)
        {
            callingConvention = convention;
        }
        inline std::string getCallingConvention() const { return callingConvention; }

        // 核心：通用DLL函数调用模板 //
        template<typename ReturnType, typename... Args>
        ReturnType callWinDll(const std::string& funcName, Args&&... args);

        void help();
    };

    /*********************** MinGW | VS 相同 ***********************/

    // 添加临时环境变量，由AddTempPath-> addTempEnv //
    void addTempEnv(const std::string& path, const std::string& envName="PATH");
    void addTempEnv(const std::wstring& newPath, const std::wstring& envName=L"PATH");

    // 后台打开文件夹 //
    void openDir(const std::string& path);

    // 获取文件列表 //
    vector<string> listDir(const string& path);


    /******************* 调用powershell--类 *******************/
    class PowerShellSession
    {
    private:
        HANDLE hChildStdinRd, hChildStdinWr;  // 用于向子进程写入
        HANDLE hChildStdoutRd, hChildStdoutWr; // 用于从子进程读取
        HANDLE hProcess;

        bool autoClose = true;

    private:
        bool _start();
        void _stop();

    public:
        PowerShellSession(bool auto_close=true)
        {
            autoClose = auto_close;
            _start();
        }

        std::string runPowerShellCommand(const std::string& command);

        ~PowerShellSession()
        {
            string now_file = __FILE__;
            string file_path = __FILE__;
            string func = __func__;
            long line = __LINE__;

            std::replace(now_file.begin(), now_file.end(), '\\', '/');
            size_t pos = now_file.find_last_of('/');
            if (pos != string::npos) {
                now_file = now_file.substr(pos + 1);
            }


            std::string text_fmt = R"([{}] [in function:{}] [{}:{}] PowerShellSession not closed!)";
            string text = wtl::format(text_fmt, file_path, func, now_file, line);

            autoClose ? _stop() : wtl::println(wtl::Color("red"), text);
        }
    };



    /******************* 调用command--类 *******************/
    class CommandSession
    {
    private:
        HANDLE hChildStdinRd, hChildStdinWr;  // 用于向子进程写入
        HANDLE hChildStdoutRd, hChildStdoutWr; // 用于从子进程读取
        HANDLE hProcess;

        bool autoClose = true;

    private:
        bool _start();
        void _stop();

    public:
        CommandSession(bool auto_close=true)
        {
            autoClose = auto_close;
            _start();
        }

        std::string runCommand(const std::string& command);

        ~CommandSession()
        {
            string now_file = __FILE__;
            string file_path = __FILE__;
            string func = __func__;
            long line = __LINE__;

            std::replace(now_file.begin(), now_file.end(), '\\', '/');
            size_t pos = now_file.find_last_of('/');
            if (pos != string::npos) {
                now_file = now_file.substr(pos + 1);
            }

            std::string text_fmt = R"([{}] [in function:{}] [{}:{}] Command session not closed!)";
            string text = wtl::format(text_fmt, file_path, func, now_file, line);

            autoClose ? _stop() : wtl::println(wtl::Color("red"), text);
        }
    };

}


// 核心：通用DLL函数调用模板 //
template<typename ReturnType, typename... Args>
ReturnType wtl::CallWinDll::callWinDll(const std::string& funcName, Args&&... args)
{
    if (!hDll)
    {
        wtl::ThrowError::showError("DLL not loaded or failed to load.");
    }

    FARPROC funcAddr = GetProcAddress(hDll, funcName.c_str());
    if (!funcAddr)
    {
        DWORD error = GetLastError();
        wtl::ThrowError::showError("Failed to get function address for \"" + funcName + "\". Error code: " + std::to_string(error));
    }

    // 关键：根据存储的callingConvention字符串，决定使用哪种函数指针类型 //
    if (callingConvention == "cdecl")
    {
        using FuncType = ReturnType(__cdecl*)(Args...);
        auto func = reinterpret_cast<FuncType>(funcAddr);
        return func(std::forward<Args>(args)...);
    }
    else if (callingConvention == "stdcall")
    {
        // 这是最常见的Windows API和很多DLL的约定
        using FuncType = ReturnType(__stdcall*)(Args...);
        auto func = reinterpret_cast<FuncType>(funcAddr);
        return func(std::forward<Args>(args)...);
    }
    else if (callingConvention == "fastcall")
    {
        using FuncType = ReturnType(__fastcall*)(Args...);
        auto func = reinterpret_cast<FuncType>(funcAddr);
        return func(std::forward<Args>(args)...);
    }
    else if (callingConvention == "thiscall")
    {
        // 注意：thiscall通常用于C++成员函数，动态调用DLL导出函数极少使用
        using FuncType = ReturnType(__thiscall*)(Args...);
        auto func = reinterpret_cast<FuncType>(funcAddr);
        return func(std::forward<Args>(args)...);
    }
    else
    {
        // 如果传入的约定名称无法识别，可以抛出异常或回退到默认
        wtl::ThrowError::showError("Unsupported calling convention: " + callingConvention);
    }
}

/****************** 查找列表元素 ******************/
template<class T>
bool wtl::findInVec(const std::vector<T>& vec, const T& target)
{
    return std::find(vec.begin(), vec.end(), target) != vec.end();
}

// 回调清理函数 //
void exitCheckWork();
