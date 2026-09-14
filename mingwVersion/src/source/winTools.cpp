#include <iostream>
#include <windows.h>
#include <cstdio>
#include <ctime>
#include <regex>  // 正则表达头文件
#include <vector>  // 自动管理内存
#include <thread>
#include <functional>
#include <cctype>
#include <chrono>
#include <gdiplus.h>
#include <tchar.h>
#include <interception.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include "winToolsH.hpp"

#pragma comment(lib, "user32.lib")

using namespace std;
using namespace cv;
namespace fs = std::filesystem;

static wtl::StringCode scd_cpp;

// 定义末尾清理注册标志（extern 声明于 winToolsH.hpp）
bool autoClear = false;

namespace wtl
{
    /************** 获取屏幕缩放 **************/
    // 获取主显示器的缩放比例
    float getScreenZoom()
    {
        setAutoDpi();
        HDC hdc = GetDC(nullptr);
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
    
        float scalingRatio = static_cast<float>(dpiX) / 96.0f;
        float roundedRatio = static_cast<float>(static_cast<int>(scalingRatio * 1000 + 0.5f)) / 1000.0f;
        return roundedRatio;
    }
    
} // namespace wtl

/************** 屏幕事件 **************/
ScreenSize getScreenSize(bool out_put)
{
    // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标
    SetProcessDPIAware();

    ScreenSize screen_size;
    int x_len, y_len;
    x_len = GetSystemMetrics(SM_CXSCREEN);  // 获取x长度
    y_len = GetSystemMetrics(SM_CYSCREEN);  // 获取y长度
    float screenZoom = wtl::getScreenZoom();
    screen_size.x = x_len;  // 结构体储存大小(屏幕)
    screen_size.y = y_len;  // 结构体储存大小(屏幕)
    screen_size.screenZoom = screenZoom;
    if (out_put)
    {
        wtl::println("Display size: (x: {}, y: {} screenZoom: {})", screen_size.x, screen_size.y, screen_size.screenZoom);
    }
    return screen_size;
    //    If you want to get coordinates you need to instantiate the struct function:
    //    ScreenSizeGet screen_size = GetScreenSIze; screen_size.x, screen_size.y
}


namespace wtl
{
    // 获取某个文件夹的所有文件，wtl命名空间，不需要考虑gbk //
    vector<string> listDir(const std::string& path)
    {
        std::vector<std::string> entries;
        try
        {
            // 检查路径有效性
            if (!fs::exists(path) || !fs::is_directory(path))
            {
                std::cerr << "Invalid path or non-directory: " << path << std::endl;
                return entries;
            }
    
            // 遍历当前层级目录（不递归）
            for (const auto& entry : fs::directory_iterator(path))
            {
                // 直接获取条目名称（含扩展名）
                std::string name = entry.path().filename().string();
                entries.push_back(name);
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "File system error: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Unknown error" << std::endl;
        }
        return entries;
    }
    
    /**************************** 加密与解密 ****************************/
    // 加密函数，返回16进制字符串 //
    string addStringKey(const std::string &text, const std::string &key)
    {
        if (key.empty()) return "";
    
        std::string result = text;
        size_t key_len = key.length();
        size_t key_index = 0;
    
        // 将字符串视为字节流进行加密
        for (size_t i = 0; i < result.length(); ++i)
        {
            // 对每个字节进行简单的XOR加密
            result[i] = result[i] ^ key[key_index];
            key_index = (key_index + 1) % key_len;
        }
    
        // 转换为16进制字符串
        std::stringstream hex_stream;
        hex_stream << std::hex << std::setfill('0');
        for (unsigned char c : result) {
            hex_stream << std::setw(2) << static_cast<int>(c);
        }
    
        return hex_stream.str();
    }
    
    // 解密函数，输入是16进制字符串 //
    std::string decryptStringKey(const std::string& hex_text, const std::string& key)
    {
        if (key.empty() || hex_text.length() % 2 != 0) return "";
    
        // 1. 将16进制字符串转换回字节
        std::string encrypted_bytes;
        for (size_t i = 0; i < hex_text.length(); i += 2) {
            std::string byte_str = hex_text.substr(i, 2);
            char byte = static_cast<char>(std::stoi(byte_str, nullptr, 16));
            encrypted_bytes.push_back(byte);
        }
    
        // 2. 解密字节
        std::string result = encrypted_bytes;
        size_t key_len = key.length();
        size_t key_index = 0;
    
        for (size_t i = 0; i < result.length(); ++i)
        {
            result[i] = result[i] ^ key[key_index];
            key_index = (key_index + 1) % key_len;
        }
    
        return result;
    }
    
    
    // 获取注册表键值 //
    string getRegistryPath(const std::string& keyPath, const std::string& valueName)
    {
        // 使用宽字符字符串
        HKEY hRootKey = HKEY_CURRENT_USER;
        fs::path lpSubKeyFs = keyPath;
        fs::path lpValueNameFs = valueName;
    
        StringCode Scd;
    
        LPCWSTR lpSubKey = lpSubKeyFs.wstring().c_str();
        LPCWSTR lpValueName = lpValueNameFs.wstring().c_str();
    
        DWORD dwBufferSize = 0;
        LONG lResult;
    
        // 第一次调用，获取所需缓冲区大小
        lResult = RegGetValueW(hRootKey, lpSubKey, lpValueName, RRF_RT_REG_SZ, nullptr, nullptr, &dwBufferSize);
        if (lResult != ERROR_SUCCESS)
        {
            std::wcerr << L"无法获取值的大小或键不存在。错误代码: " << lResult << std::endl;
            return "";
        }
    
        // 分配宽字符缓冲区
        std::wstring data;
        data.resize(dwBufferSize / sizeof(wchar_t));
    
        // 第二次调用，实际读取数据
        lResult = RegGetValueW(hRootKey, lpSubKey, lpValueName, RRF_RT_REG_SZ, nullptr, &data[0], &dwBufferSize);
        if (lResult == ERROR_SUCCESS)
        {
            // 处理字符串终止符
            if (dwBufferSize > 0)
            {
                // 确保字符串正确终止
                DWORD charCount = dwBufferSize / sizeof(wchar_t);
                if (charCount > 0 && data[charCount - 1] == L'\0')
                {
                    data.resize(charCount - 1);
                }
            }
            fs::path filePath = data;
            return filePath.string();
            //        std::wcout << L"读取到的值: " << data << std::endl;
        }
        else
        {
            std::wcerr << L"读取注册表值失败。错误代码: " << lResult << std::endl;
            return "";
        }
    }
    
    // 打开其他软件 //
    bool startOtherApp(const std::string& appPath)
    {
        std::filesystem::path pathObj(appPath);
        std::wstring widePath = pathObj.wstring();
        std::wstring wideWorkingDir = pathObj.parent_path().wstring();
    
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
    
        BOOL success = CreateProcessW(
                widePath.c_str(),
                NULL,
                NULL,
                NULL,
                FALSE,
                CREATE_NEW_CONSOLE,
                NULL,
                wideWorkingDir.c_str(),  // 直接指定新进程的工作目录
                &si,
                &pi
        );
    
        if (success)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }
        return false;
    }
    
    bool startOtherApp(const string& appPath, bool console)
    {
        if (console)
        {
            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi = { 0 };
    
            fs::path wString = appPath;
    
            BOOL success = CreateProcessW(
                    wString.c_str(),
                    NULL,                           // 命令行参数
                    NULL,                           // 进程安全性
                    NULL,                           // 线程安全性
                    FALSE,                          // 不继承句柄
                    0,                              // 创建标志
                    NULL,                           // 环境变量
                    NULL,                           // 当前目录
                    &si,
                    &pi
            );
    
            if (success)
            {
                // 持有子进程句柄，父进程退出前可选择等待或终止子进程
                std::cout << "Success! PID: " << pi.dwProcessId << std::endl;
    
                // 示例：等待子进程结束（阻塞）或进行其他操作
                WaitForSingleObject(pi.hProcess, INFINITE);
    
                // 当父进程准备退出时，终止子进程
                TerminateProcess(pi.hProcess, 0);
    
                // 关闭句柄
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
    
                return true;  // 添加返回值
            }
            else
            {
                std::cerr << "create error code: " << GetLastError() << std::endl;
                return false;  // 返回false
            }
        }
        else
        {
            bool result = startOtherApp(appPath);  // 这里调用的是另一个重载版本？
            return result;
        }
    }
    
    
    
    // 删除文件 //
    bool FileManagement::removeFile(const std::string& fileName)
    {
        StringCode scd;
        std::wstring wfileName = fs::u8path(fileName).wstring();
        const wchar_t* filePath = wfileName.c_str();
    
        // 去掉只读属性（可选）
        DWORD attrs = GetFileAttributesW(filePath);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY))
        {
            SetFileAttributesW(filePath, attrs & ~FILE_ATTRIBUTE_READONLY);
        }
    
        // 直接删除文件
        if (DeleteFileW(filePath))
        {
            return true;
        }
        else
        {
            std::cerr << scd.utf8ToGbk("删除失败，错误代码: ") << GetLastError() << std::endl;
            return false;
        }
    }
    
    
    // 创建文件夹 //
    bool FileManagement::createDir(const std::string& fileName)
    {
        // 要创建的文件夹路径（UTF-8 -> 宽字符）
        wstring dirName = fs::u8path(fileName).wstring();
    
        LPCWSTR folderPath = dirName.c_str();
    
        // 调用 Windows API 创建文件夹
        BOOL result = CreateDirectoryW(
                folderPath,        // 文件夹路径
                NULL                // 安全属性（可选参数，设为NULL使用默认）
        );
    
        // 检查是否创建成功
        if (result)
        {
            return true;
        }
        else
        {
            // 获取错误代码并提示
            return false;
        }
    
    }
    
    
    // 删除文件夹 //
    bool FileManagement::removeDir(const std::string& dirPath)
    {
        try
        {
            // 检查路径是否存在且是目录
            if (fs::exists(dirPath) && fs::is_directory(dirPath))
            {
                // 递归删除目录及其内容
                uintmax_t n = fs::remove_all(dirPath);
                return true;
            }
            else
            {
                std::cerr << "Path does not exist or is not a directory: " << scd_cpp.utf8ToGbk(dirPath) << "\n";
                return false;
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Filesystem error\n";
            std::cerr << "Path: " << e.path1() << "\n";
            return false;
        }
        catch (const std::exception& e)
        {
            std::cerr << "General error\n";
            return false;
        }
        return true;
    }
    
    
    // 写入日志 //
    bool FileManagement::writeLog(const std::string& str, const std::string& fileName)
    {
        // 1. 使用正确的文件名参数打开文件（UTF-8 -> 宽字符） //
        FILE* file = _wfopen(fs::u8path(fileName).wstring().c_str(), L"a");
    
        if (file == nullptr)
        {
            printf("Unable to open the log file!\n");
            return false;
        }
    
        // 2. 正确写入传入的日志内容（自动追加换行符） //
        fprintf(file, "%s\n", str.c_str());  // 关键修改：使用str内容
    
        // 3. 必须关闭文件释放资源 //
        fclose(file);  // 关键补充：防止资源泄漏
    
        return true;
    }
    
    
    bool FileManagement::copyDir(const fs::path& sourcePath, const fs::path& targetPath)
    {
        try
        {
            // 检查源路径
            if (!fs::exists(sourcePath))
            {
                std::cerr << "Source path does not exist: " + sourcePath.string();
                return false;
            }
            if (!fs::is_directory(sourcePath))
            {
                std::cerr << "Source path is not a directory: " + sourcePath.string();
                return false;
            }
    
            // 在目标路径下创建同名文件夹
            auto targetDir = targetPath / sourcePath.filename(); // 关键修改：添加同名子目录
            fs::create_directories(targetDir);
    
            // 递归复制内容
            for (const auto& entry : fs::directory_iterator(sourcePath))
            {
                const auto& path = entry.path();
                auto target = targetDir / path.filename(); // 目标路径修正
    
                if (fs::is_directory(path))
                {
                    // 递归复制子目录
                    copyDir(path, targetDir);
                }
                else if (fs::is_regular_file(path))
                {
                    fs::copy_file(path, target, fs::copy_options::overwrite_existing);
                }
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "File system error: " << e.what() << '\n';
            return false;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Standard exception: " << e.what() << '\n';
            return false;
        }
        return true;
    }
    
    
    std::string FileManagement::getFileTree(const std::string& rootPath)
    {
        std::string result;
        result.reserve(1024 * 100);  // 预分配 100KB
    
        // 1. 输出根目录路径，并统一替换为 '/'
        std::string displayRootPath = rootPath;
        std::replace(displayRootPath.begin(), displayRootPath.end(), '\\', '/');
        result += displayRootPath + "/\n";  // 确保末尾有斜杠，表示这是一个目录
    
        std::function<void(const fs::path&, int)> walkDir;
        walkDir = [&](const fs::path& dir, int depth) {
            try {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    auto filename = entry.path().filename().u8string();
    
                    // 2. 构建缩进和条目行
                    result += std::string(depth * 2, ' ') + filename;
    
                    if (fs::is_directory(entry.status())) {
                        result += "/\n";
                        walkDir(entry.path(), depth + 1);
                    } else {
                        result += "\n";
                    }
                }
            } catch (...) {}
        };
    
        walkDir(fs::u8path(rootPath), 0);
        return result;
    }
    
    /****************** 检查控制台编码 ******************/
    string getConsoleEncoding()
    {
        // 获取控制台输出代码页
        UINT console_cp = GetConsoleOutputCP();
    
        // 获取系统默认 ANSI 代码页
        UINT ansi_cp = GetACP();
    
        // 获取系统默认 OEM 代码页（控制台默认）
        UINT oem_cp = GetOEMCP();
    
        // 将代码页转换为字符串描述
        if (console_cp == 65001) return "utf8";
        if (console_cp == 936) return "gbk";
        if (console_cp == 950) return "big5";
    
        // 返回其他代码页的数值形式
        return "Code Page: " + std::to_string(console_cp);
    }
    
    /****************** 锁定窗口 ******************/
    static HWND _findWindowByProcessId(DWORD targetProcessId)
    {
        HWND resultHwnd = nullptr;
    
        // 使用 lambda 作为回调函数
        auto enumProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto& data = *reinterpret_cast<std::pair<DWORD, HWND*>*>(lParam);
            DWORD processId = 0;
    
            GetWindowThreadProcessId(hwnd, &processId);
    
            if (processId == data.first && IsWindowVisible(hwnd))
            {
                *data.second = hwnd;  // 保存窗口句柄
                return false;         // 停止枚举
            }
            return true;              // 继续枚举
        };
    
        std::pair<DWORD, HWND*> data = { targetProcessId, &resultHwnd };
        EnumWindows(enumProc, reinterpret_cast<LPARAM>(&data));
    
        return resultHwnd;
    }
    
    
    bool lockWindows(const std::string &windowName)
    {
        fs::path winName = windowName;
        wstring str = winName.wstring();
        // 查找句柄 //
        HWND hWnd = FindWindowW(nullptr, winName.wstring().c_str());
        if (hWnd != nullptr)
        {
            // 将窗口设置为前台
            bool result = SetForegroundWindow(hWnd);
            return result;
        }
        else
        {
            return false;
        }
    }
    
    bool lockWindows(int pid)
    {
        // 查找句柄 //
        HWND hWnd = _findWindowByProcessId(pid);
        if (hWnd != nullptr)
        {
            // 将窗口设置为前台
            bool result = SetForegroundWindow(hWnd);
            return result;
        }
        else
        {
            return false;
        }
    }
    
    
    /******************** 开机自启 ******************/
    bool setAutoStartUp(const fs::path &appName, const fs::path& exePath)
    {
        HKEY hKey = nullptr;
    
        // 打开注册表键
        LONG result = RegOpenKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0,
                KEY_SET_VALUE,
                &hKey
        );
    
        if (result != ERROR_SUCCESS) {
            return false;
        }
    
        // 获取宽字符串路径 //
        std::wstring exePathStr = exePath.wstring();
    
        // 将程序路径写入注册表
        result = RegSetValueExW(
                hKey,
                appName.wstring().c_str(),
                0,
                REG_SZ,
                (const BYTE*)exePathStr.c_str(),
                (exePathStr.length() + 1) * sizeof(wchar_t)
        );
    
        RegCloseKey(hKey);
    
        return (result == ERROR_SUCCESS);
    }
    
    // exe本身路径 //
    bool setAutoStartUp(const fs::path &appName)
    {
        // 获取exe当前路径 //
        fs::path exeNowPath = format(R"({}\{})",getExePath(),getNowExeName());
    
        return setAutoStartUp(appName,exeNowPath);
    }
    
    bool setAutoStartUpCmd(const fs::path& appName, const fs::path& exePath)
    {
        HKEY hKey = nullptr;
    
        // 获取程序目录和文件名
        fs::path fileDir = exePath.parent_path().wstring();
        fs::path fileName = exePath.filename().wstring();
    
        // 构建cmd命令字符串，先切换到目录再启动程序
        fs::path cmdPath = format(R"(cmd /c "cd /d "{0}" && start "" "{1}"")",
                                       fileDir, fileName);
    
        // 打开注册表键
        LONG result = RegOpenKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0,
                KEY_SET_VALUE,
                &hKey
        );
    
        if (result != ERROR_SUCCESS) {
            return false;
        }
    
        // 将cmd命令路径写入注册表
        result = RegSetValueExW(
                hKey,
                appName.wstring().c_str(),
                0,
                REG_SZ,
                (const BYTE*)cmdPath.c_str(),
                (cmdPath.wstring().length() + 1) * sizeof(wchar_t)
        );
    
        RegCloseKey(hKey);
    
        return (result == ERROR_SUCCESS);
    }
    
    // 获取开机自启的存放路径 //
    std::string getAutoStartExePath(const fs::path &appName)
    {
        HKEY hKey = nullptr;
        std::string resultPath = "";
    
        // 打开注册表键
        LONG result = RegOpenKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0,
                KEY_QUERY_VALUE,
                &hKey
        );
    
        if (result != ERROR_SUCCESS) {
            return resultPath;
        }
    
        // 获取值的大小
        DWORD dataSize = 0;
        result = RegQueryValueExW(
                hKey,
                appName.wstring().c_str(),
                nullptr,
                nullptr,
                nullptr,
                &dataSize
        );
    
        if (result == ERROR_SUCCESS && dataSize > 0) {
            // 分配缓冲区
            std::wstring buffer(dataSize / sizeof(wchar_t), L'\0');
    
            // 读取注册表值
            result = RegQueryValueExW(
                    hKey,
                    appName.wstring().c_str(),
                    nullptr,
                    nullptr,
                    (LPBYTE)buffer.data(),
                    &dataSize
            );
    
            if (result == ERROR_SUCCESS) {
                // 移除可能的终止符
                if (!buffer.empty() && buffer.back() == L'\0') {
                    buffer.pop_back();
                }
    
                // 转换为fs::path并获取父目录
                fs::path exePath(buffer);
                fs::path parentDir = exePath.parent_path();
    
                // 转换为string
                if (!parentDir.empty())
                {
                    resultPath = parentDir.string();
                }
            }
        }
    
        RegCloseKey(hKey);
        return resultPath;
    }
    
    // 删除开机自启 //
    bool disableAutoStart(const fs::path& appName)
    {
        HKEY hKey = nullptr;
    
        // 打开注册表键
        LONG result = RegOpenKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0,
                KEY_SET_VALUE,
                &hKey
        );
    
        if (result != ERROR_SUCCESS) {
            return false;
        }
    
        // 获取应用程序名称的宽字符串
        std::wstring appNameWstr = appName.wstring();
    
        // 删除注册表项
        result = RegDeleteValueW(
                hKey,
                appNameWstr.c_str()
        );
    
        RegCloseKey(hKey);
    
        return (result == ERROR_SUCCESS);
    }
    
    // 输入函数 //
    string input(const string &inform_text,const string &color)
    {
        if (!inform_text.empty())
        {
            color.empty()?print(inform_text):print(Color(color),inform_text);
        }
        string get_text;
        std::getline(cin,get_text);
        return get_text;
    }
    
    
    /************************ 其他 ************************/
    bool PowerShellSession::_start()
    {
        SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    
        // 创建 stdin 管道
        if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) return false;
        SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
    
        // 创建 stdout 管道
        if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) return false;
        SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
    
        PROCESS_INFORMATION pi;
        STARTUPINFOW si = {sizeof(si)};
        si.hStdInput = hChildStdinRd;
        si.hStdOutput = hChildStdoutWr;
        si.hStdError = hChildStdoutWr;
        si.dwFlags = STARTF_USESTDHANDLES;
    
        std::wstring cmdLine = L"powershell.exe -NoProfile -NonInteractive -Command -";
    
        BOOL success = CreateProcessW(NULL, &cmdLine[0], NULL, NULL, TRUE,
                                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (!success) return false;
    
        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(hChildStdinRd);
        CloseHandle(hChildStdoutWr);
    
        return true;
    }
    
    
    void PowerShellSession::_stop()
    {
        if (hProcess)
        {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
        CloseHandle(hChildStdinWr);
        CloseHandle(hChildStdoutRd);
    }
    
    std::string PowerShellSession::runPowerShellCommand(const std::string& command)
    {
        std::string result;
        if (hChildStdinWr == INVALID_HANDLE_VALUE || hChildStdoutRd == INVALID_HANDLE_VALUE)
            return result;
    
        // 发送命令并添加结束标记
        std::string marker = "CMD_DONE_" + std::to_string(GetTickCount64());
        std::string fullCmd = command + "\r\nWrite-Host '" + marker + "'\r\n";
    
        DWORD written;
        if (!WriteFile(hChildStdinWr, fullCmd.c_str(), fullCmd.size(), &written, NULL))
            return result;
    
        // 读取直到找到结束标记
        char buffer[4096];
        DWORD bytesRead;
    
        while (true) {
            if (ReadFile(hChildStdoutRd, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
    
                // 检查是否收到结束标记
                if (result.find(marker) != std::string::npos) {
                    // 移除标记行
                    size_t pos = result.find("\r\n" + marker);
                    if (pos != std::string::npos) {
                        result = result.substr(0, pos);
                    }
                    break;
                }
            } else {
                // 读取失败或管道关闭
                break;
            }
        }
    
        return result;
    }
    
    
    bool CommandSession::_start()
    {
        SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    
        // 创建 stdin 管道
        if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) return false;
        SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
    
        // 创建 stdout 管道
        if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) return false;
        SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
    
        PROCESS_INFORMATION pi;
        STARTUPINFOW si = {sizeof(si)};
        si.hStdInput = hChildStdinRd;
        si.hStdOutput = hChildStdoutWr;
        si.hStdError = hChildStdoutWr;
        si.dwFlags = STARTF_USESTDHANDLES;
    
        // 🔥 关键改动：将 powershell.exe 改为 cmd.exe
        std::wstring cmdLine = L"cmd.exe";
    
        BOOL success = CreateProcessW(NULL, &cmdLine[0], NULL, NULL, TRUE,
                                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (!success) return false;
    
        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(hChildStdinRd);
        CloseHandle(hChildStdoutWr);
    
        return true;
    }
    
    void CommandSession::_stop()
    {
        if (hProcess)
        {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
        CloseHandle(hChildStdinWr);
        CloseHandle(hChildStdoutRd);
    }
    
    std::string CommandSession::runCommand(const std::string& command)
    {
        std::string result;
        if (hChildStdinWr == INVALID_HANDLE_VALUE || hChildStdoutRd == INVALID_HANDLE_VALUE)
            return result;
    
        // 发送命令并添加结束标记
        std::string marker = "CMD_DONE_" + std::to_string(GetTickCount64());
        std::string fullCmd = command + "\r\necho " + marker + "\r\n";
    
        DWORD written;
        if (!WriteFile(hChildStdinWr, fullCmd.c_str(), fullCmd.size(), &written, NULL))
            return result;
    
        // 读取直到找到结束标记
        char buffer[4096];
        DWORD bytesRead;
    
        while (true) {
            if (ReadFile(hChildStdoutRd, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
    
                // 检查是否收到结束标记
                if (result.find(marker) != std::string::npos) {
                    // 移除标记行
                    size_t pos = result.find("\r\n" + marker);
                    if (pos != std::string::npos) {
                        result = result.substr(0, pos);
                    }
                    break;
                }
            } else {
                break;
            }
        }
    
        return result;
    }
    
    
    // 自动调整dpi //
    void setAutoDpi()
    {
        // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标 //
        SetProcessDPIAware();
    }
    
    
    /****************** 判断是否为管理员 ******************/
    bool isAdmin()
    {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            return false;
    
        TOKEN_ELEVATION elevation;
        DWORD dwSize;
        bool bIsAdmin = false;
    
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize))
        {
            bIsAdmin = elevation.TokenIsElevated;
        }
    
        CloseHandle(hToken);
        return bIsAdmin;
    }
    
    bool openAdmin()
    {
        WCHAR szPath[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, szPath, MAX_PATH))
        {
            std::cerr << "GetModuleFileName failed (" << GetLastError() << ")\n";
            return false;
        }
    
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.nShow = SW_SHOWNORMAL;
        sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    
        if (!ShellExecuteExW(&sei))
        {
            DWORD err = GetLastError();
            if (err == ERROR_CANCELLED)
                std::cerr << "User refused UAC prompt.\n";
            else
                std::cerr << "ShellExecuteEx failed (" << err << ")\n";
            return false;
        }
        return true;
    }
    
    double getRunTime()
    {
        return std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
    
    
    /***************** 调试输出 *****************/
    // 调试输出辅助函数 //
    string debugMessage(const string &message,
                             const char* file,
                             const char* func_name,
                             int line,
                        const string &debug_model,
                        const string &output_color
    )
    {
        // 获取当前时间戳（带毫秒） //
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        char timeStr[24];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);
        std::ostringstream oss;
        oss << timeStr << "." << std::setfill('0') << std::setw(3) << ms.count();
        string timestamp = oss.str();
    
        // 获取当前文件名称（只保留文件名，去掉路径） //
        string now_file = file;
        std::replace(now_file.begin(), now_file.end(), '\\', '/');
        size_t pos = now_file.find_last_of('/');
        if (pos != string::npos) {
            now_file = now_file.substr(pos + 1);
        }
    
        // 输出格式：[时间] [函数名] [文件名:行号] [模式] 信息 //
        string output_message_format = "[{}] [in function:{}] [{}:{}] [{}] {}";
    
        string output_message = format(
                output_message_format,
                timestamp,
                func_name,
                now_file,line,
                debug_model,
                message
        );
        println(Color(output_color), output_message);
        return output_message;
    }
    
    std::string getNowExeName()
    {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    
        // 使用宽字符路径
        fs::path exePath = buffer;
    
        // 转换为UTF-8字符串 //
        std::string utf8Str = exePath.filename().u8string();
        return utf8Str;
    }
    
    
    
    void addTempEnv(const std::string& path, const std::string& envName)
    {
        // 将路径转为宽字符串
        std::wstring temp_path = fs::path(path).wstring();
    
        std::wstring wEnvName = fs::path(envName).wstring();
    
        // 获取当前指定环境变量的值
        std::vector<wchar_t> buffer(32767);
        DWORD len = GetEnvironmentVariableW(wEnvName.c_str(), buffer.data(), 32767);
    
        // 构造新的环境变量值
        std::wstring newEnvValue = temp_path + L";" + std::wstring(buffer.data(), len);
    
        // 设置新的环境变量值
        SetEnvironmentVariableW(wEnvName.c_str(), newEnvValue.c_str());
    }
    
    void addTempEnv(const std::wstring& newPath,const std::wstring& envName)
    {
        // 获取当前指定环境变量的值
        std::vector<wchar_t> buffer(32767);
        DWORD len = GetEnvironmentVariableW(envName.c_str(), buffer.data(), 32767);
    
        // 构造新的环境变量值
        std::wstring newEnvValue = newPath + L";" + std::wstring(buffer.data(), len);
    
        // 设置新的环境变量值
        SetEnvironmentVariableW(envName.c_str(), newEnvValue.c_str());
    }
    
    void useConsoleUtf8()
    {
        // 设置输入输出都为UTF8 //
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
    
    // Function: Inject DLL into target process (wide character version)
    bool loadToMemory(DWORD processId, const fs::path &dll_path)
    {
        const wchar_t *dllPath = dll_path.wstring().c_str();
    
        // 1. Open target process
        HANDLE hProcess = OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                FALSE, processId);
    
        if (hProcess == NULL) {
            std::cerr << "Failed to open process. Error: " << GetLastError() << std::endl;
            return false;
        }
    
        // 2. Allocate memory in target process for DLL path
        size_t dllPathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
        LPVOID pRemoteMemory = VirtualAllocEx(hProcess, NULL, dllPathSize,
                                              MEM_COMMIT, PAGE_READWRITE);
        if (pRemoteMemory == NULL) {
            std::cerr << "Failed to allocate remote memory. Error: " << GetLastError() << std::endl;
            CloseHandle(hProcess);
            return false;
        }
    
        // 3. Write DLL path to target process memory
        SIZE_T bytesWritten;
        if (!WriteProcessMemory(hProcess, pRemoteMemory, dllPath,
                                dllPathSize, &bytesWritten)) {
            std::cerr << "Failed to write to remote memory. Error: " << GetLastError() << std::endl;
            VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
    
        // 4. Get address of LoadLibraryW function
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (hKernel32 == NULL) {
            std::cerr << "Failed to get handle of kernel32.dll. Error: " << GetLastError() << std::endl;
            VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
    
        LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryW");
        if (pLoadLibrary == NULL) {
            std::cerr << "Failed to get address of LoadLibraryW. Error: " << GetLastError() << std::endl;
            VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
    
        // 5. Create remote thread in target process
        HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0,
                                                  (LPTHREAD_START_ROUTINE)pLoadLibrary,
                                                  pRemoteMemory, 0, NULL);
    
        if (hRemoteThread == NULL) {
            std::cerr << "Failed to create remote thread. Error: " << GetLastError() << std::endl;
            VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
    
        // 6. Wait for thread to complete
        WaitForSingleObject(hRemoteThread, INFINITE);
    
        // Check if DLL loaded successfully
        DWORD exitCode = 0;
        bool success = true;
    
        if (!GetExitCodeThread(hRemoteThread, &exitCode)) {
            std::cerr << "Failed to get thread exit code. Error: " << GetLastError() << std::endl;
            success = false;
        } else if (exitCode == 0) {
            std::cerr << "DLL injection failed. LoadLibraryW returned NULL." << std::endl;
            success = false;
        }
    
        // 7. Clean up resources
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hRemoteThread);
        CloseHandle(hProcess);
    
        return success;
    }
    
    // 关闭控制台 //
    void hideConsole()
    {
        // 分离当前进程与控制台 //
        FreeConsole();
    }
    
    // 打开控制台 //
    void showConsole()
    {
        // 尝试重新附加到父控制台
        if (AttachConsole(ATTACH_PARENT_PROCESS))
        {
            ;
        }
        else
        {
            // 如果没有父控制台，创建新的 //
            AllocConsole();
        }
    
    }
    
    /************** 查找字符串行数 *************/
    // 查找字符串行数 //
    long SearchText::findLine(const string &text, const string &pattern)
    {
        std::vector<string> lines = stringToVector(text);
        long count = 1;
        for (const string &line : lines)
        {
            if (!re::search(pattern, line).empty())
            {
                return count;
            }
            ++count;
        }
        return -1;
    }

    std::vector<long> SearchText::findLines(const string &text, const string &pattern)
    {
        std::vector<long> resultList;
        std::vector<string> lines = stringToVector(text);
        long count = 1;
        for (const string &line : lines)
        {
            if (!re::search(pattern, line).empty())
            {
                resultList.push_back(count);
            }
            ++count;
        }
        return resultList;
    }

    string SearchText::findLineContent(const string &text, const string &pattern)
    {
        std::vector<string> lines = stringToVector(text);
        for (const string &line : lines)
        {
            if (!re::search(pattern, line).empty())
            {
                return line;
            }
        }
        return "";
    }

    std::vector<string> SearchText::findLinesContent(const string &text, const string &pattern)
    {
        std::vector<string> resultList;
        std::vector<string> lines = stringToVector(text);
        for (const string &line : lines)
        {
            if (!re::search(pattern, line).empty())
            {
                resultList.push_back(line);
            }
        }
        return resultList;
    }

    wtl::Dict<int, string> SearchText::findLinesMap(const string &text, const string &pattern)
    {
        wtl::Dict<int, string> resultMap;
        std::vector<string> lines = stringToVector(text);
        int count = 1;
        for (const string &line : lines)
        {
            if (!re::search(pattern, line).empty())
            {
                resultMap[count] = line;
            }
            ++count;
        }
        return resultMap;
    }

    std::vector<wtl::Dict<int, string>> SearchText::findLinesMapEach(const string &text, const string &pattern)
    {
        std::vector<wtl::Dict<int, string>> resultList;
        std::vector<string> lines = stringToVector(text);
        int count = 1;
        for (const string &line : lines)
        {
            if (!re::search(pattern, line).empty())
            {
                wtl::Dict<int, string> singleResult;
                singleResult[count] = line;
                resultList.push_back(singleResult);
            }
            ++count;
        }
        return resultList;
    }

    /*****************************/
    
    // 字符串转容器,根据行数转化 //
    std::vector<std::string> stringToVector(const std::string& str)
    {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string line;
    
        while (std::getline(ss, line))
        {
            result.push_back(line);
        }
    
        return result;
    }
    
    void openDir(const std::string& path)
    {
        // 使用 ShellExecuteW 打开文件夹（UTF-8 -> 宽字符）
        ShellExecuteW(nullptr, L"open", fs::u8path(path).wstring().c_str(), nullptr, nullptr, SW_SHOW);
    }
    
    // 冒泡排序 //
    vector<int> sort(const int* numList, int numListSize, const string& model)
    {
        // 算法思路为左边小，右边大，左边小设置true，反之false //
        bool is_left_small = true;
        // 最终排序结果 //
        vector<int> resultList;
        // 降序临时结果 //
        vector<int>tempList;
    
        /************** 排序 **************/
    
        // 创建临时数组 //
        resultList.reserve(numListSize);
        for (int i = 0; i < numListSize; i++)
        {
            resultList.push_back(numList[i]);
        }
    
        Repeat:
        // 进行交换 //
        for (int i = 0; i < numListSize - 1; i++)
        {
            int j = (i + 1);
            // 如果左值比右值大，比如3 > 2， 交换位置 //
            if (resultList[i] > resultList[j])
            {
                //            printf("位置:i:%d, 位置:j:%d\n", tempList[i], tempList[j]);
                std::swap(resultList[j], resultList[i]);
                is_left_small = false;
            }
        }
    
        if (!is_left_small)
        {
            is_left_small = true;
            goto Repeat;
        }
    
        // 返回升序和降序 //
        if (model == "up")
        {
            return resultList;
        }
            // 降序 //
        else if (model == "down")
        {
            // 总大小，非0开始计算 //
            int size = ((int)resultList.size()) - 1;
            for (int i = size; i >= 0; i--)
            {
                tempList.push_back(resultList[i]);
            }
            return tempList;
        }
        return resultList;
    }
    
    vector<int>sort(const std::vector<int>& numList, const string& model)
    {
        // 算法思路为左边小，右边大，左边小设置true，反之false //
        bool is_left_small = true;
        // 最终排序结果 //
        vector<int> resultList;
        // 降序临时结果 //
        vector<int>tempList;
    
        /************** 排序 **************/
    
        // 创建临时数组 //
        resultList.reserve(numList.size());
        for (int i = 0; i < numList.size(); i++)
        {
            resultList.push_back(numList[i]);
        }
    
        Repeat:
        // 进行交换 //
        for (int i = 0; i < numList.size() - 1; i++)
        {
            int j = (i + 1);
            // 如果左值比右值大，比如3 > 2， 交换位置 //
            if (resultList[i] > resultList[j])
            {
                //            printf("位置:i:%d, 位置:j:%d\n", tempList[i], tempList[j]);
                std::swap(resultList[j], resultList[i]);
                is_left_small = false;
            }
        }
    
        if (!is_left_small)
        {
            is_left_small = true;
            goto Repeat;
        }
    
        // 返回升序和降序 //
        if (model == "up")
        {
            return resultList;
        }
            // 降序 //
        else if (model == "down")
        {
            // 总大小，非0开始计算 //
            int size = ((int)resultList.size()) - 1;
            for (int i = size; i >= 0; i--)
            {
                tempList.push_back(resultList[i]);
            }
            return tempList;
        }
        return resultList;
    }
    
    
    // 获取当前时间 //
    string getNowTime()
    {
        // 获取当前时间戳（秒级和毫秒级）
        auto now = std::chrono::system_clock::now();
    
    
        // 转换为本地时间（年月日时分秒）
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm local_time = *std::localtime(&time);
    
    
        // 使用strftime格式化输出
        char buffer[160];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_time);
        return buffer;
    }
    
    // 求和 //
    int sum(std::vector<int>& numList)
    {
        int num = 0;
        for (auto& i : numList)
        {
            num += i;
        }
        return num;
    }
    
    long sum(std::vector<long>& numList)
    {
        long num = 0;
        for (auto& i : numList)
        {
            num += i;
        }
        return num;
    }
    
    float sum(std::vector<float>& numList)
    {
        float num = 0;
        for (auto& i : numList)
        {
            num += i;
        }
        return num;
    }
    
    double sum(std::vector<double>& numList)
    {
        double num = 0;
        for (auto& i : numList)
        {
            num += i;
        }
        return num;
    }
    
    // 获取当前exe路径（宽字符 -> UTF-8） //
    static string _getExePath()
    {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH); // 获取exe完整路径
        fs::path exePath(buffer);
        return exePath.parent_path().u8string();  // 返回目录部分
    }
    
    string getExePath(bool useWindows)
    {
        if (useWindows)
        {
            // 统一使用宽字符 API + fs::path，无需 GBK 转码 //
            return _getExePath();
        }
    
        // 获取当前路径,如果是false //
        return fs::current_path().u8string();
    }
    
    std::string findExeInPath(const std::string& exeName,bool ignoreExe)
    {
        // 获取系统PATH
        char* pathEnv = getenv("PATH");
        if (!pathEnv) return "";
    
        std::string pathStr(pathEnv);
        std::string delimiter = ";";
        size_t pos = 0;
    
        while ((pos = pathStr.find(delimiter)) != std::string::npos)
        {
            std::string dir = pathStr.substr(0, pos);
    
            // 检查该目录下是否存在exe
            std::string fullPath = dir + "\\" + exeName;
            DWORD attrib = GetFileAttributesA(fullPath.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES &&
                !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                // 将路径中的反斜杠转为正斜杠
                std::string convertedPath = fullPath;
                std::replace(convertedPath.begin(), convertedPath.end(), '\\', '/');
                fs::path getExePath = convertedPath;
                return ignoreExe?getExePath.parent_path().string():getExePath.string();
            }
    
            pathStr.erase(0, pos + delimiter.length());
        }
    
        return "";
    }
    
    bool notRepeatOpenExe(const std::string &exeName)
    {
        // 获取进程的可执行文件名（去掉路径）
        std::wstring targetExeName = fs::path(exeName).filename().wstring(); // 例如 L"远程关机.exe"
    
        // 转换为小写用于比较（Windows 进程名不区分大小写）
        std::wstring targetExeNameLower = targetExeName;
        for (auto& c : targetExeNameLower) {
            c = std::tolower(c, std::locale());
        }
    
        // 创建进程快照
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE)
        {
            std::cerr << "Failed to create process snapshot, error code: " << GetLastError() << std::endl;
            return true; // 无法检查时默认允许
        }
    
        PROCESSENTRY32W pe32 = {0};
        pe32.dwSize = sizeof(PROCESSENTRY32W);
    
        int processCount = 0; // 统计进程数量
        std::vector<DWORD> pids; // 存储找到的进程ID（用于调试）
    
        // 遍历系统所有进程
        if (Process32FirstW(hSnapshot, &pe32))
        {
            do {
                // 直接比较进程名（不依赖完整路径获取权限）
                std::wstring currentExeName = pe32.szExeFile;
                std::wstring currentExeNameLower = currentExeName;
                for (auto& c : currentExeNameLower) {
                    c = std::tolower(c, std::locale());
                }
    
                // 比较进程名
                if (currentExeNameLower == targetExeNameLower) {
                    processCount++;
                    pids.push_back(pe32.th32ProcessID);
                }
    
            } while (Process32NextW(hSnapshot, &pe32));
        } else {
            std::cerr << "not find process error code: " << GetLastError() << std::endl;
        }
    
        CloseHandle(hSnapshot); // 关闭快照句柄
    #ifdef DEBUG
        // 调试信息：输出找到的进程数量和ID
        std::cout << "找到的进程数量: " << processCount << std::endl;
        if (processCount > 0) {
            std::cout << "进程ID列表: ";
            for (DWORD pid : pids) {
                std::cout << pid << " ";
            }
            std::cout << std::endl;
        }
    #endif
    
        // 根据您的需求修改逻辑：进程数>1时返回false，否则返回true
        if (processCount > 1)
        {
    #ifdef DEBUG
            std::cout << "检测到多个进程实例" << std::endl;
    #endif
            return false; // 存在多个进程实例
        }
        else
        {
    #ifdef DEBUG
            std::cout << "进程数正常（<=1）" << std::endl;
    #endif
            return true;  // 进程数为0或1，允许操作
        }
    }
    
    // 添加ico //
    #pragma pack(push, 1)
    struct ICONDIR {
        WORD idReserved;
        WORD idType;
        WORD idCount;
    };
    
    struct ICONDIRENTRY {
        BYTE bWidth;
        BYTE bHeight;
        BYTE bColorCount;
        BYTE bReserved;
        WORD wPlanes;
        WORD wBitCount;
        DWORD dwBytesInRes;
        DWORD dwImageOffset;
    };
    
    struct GRPICONDIR {
        WORD idReserved;
        WORD idType;
        WORD idCount;
    };
    
    struct GRPICONDIRENTRY {
        BYTE bWidth;
        BYTE bHeight;
        BYTE bColorCount;
        BYTE bReserved;
        WORD wPlanes;
        WORD wBitCount;
        DWORD dwBytesInRes;
        WORD nID;
    };
    #pragma pack(pop)
    
    // 从 ICO 文件中提取所有图标
    static std::vector<std::pair<GRPICONDIRENTRY, std::vector<BYTE>>> _extractIconsFromIco(const std::vector<BYTE>& icoData) {
        std::vector<std::pair<GRPICONDIRENTRY, std::vector<BYTE>>> icons;
    
        if (icoData.size() < sizeof(ICONDIR)) {
            return icons;
        }
    
        const ICONDIR* iconDir = reinterpret_cast<const ICONDIR*>(icoData.data());
        if (iconDir->idType != 1) {  // 必须是图标文件
            return icons;
        }
    
        const ICONDIRENTRY* entries = reinterpret_cast<const ICONDIRENTRY*>(icoData.data() + sizeof(ICONDIR));
    
        for (int i = 0; i < iconDir->idCount; i++) {
            if (entries[i].dwImageOffset + entries[i].dwBytesInRes > icoData.size()) {
                continue;
            }
    
            // 创建组图标条目
            GRPICONDIRENTRY grpEntry;
            grpEntry.bWidth = entries[i].bWidth;
            grpEntry.bHeight = entries[i].bHeight;
            grpEntry.bColorCount = entries[i].bColorCount;
            grpEntry.bReserved = entries[i].bReserved;
            grpEntry.wPlanes = entries[i].wPlanes;
            grpEntry.wBitCount = entries[i].wBitCount;
            grpEntry.dwBytesInRes = entries[i].dwBytesInRes;
            grpEntry.nID = static_cast<WORD>(i + 1);  // 图标ID从1开始
    
            // 提取图标数据
            std::vector<BYTE> iconData(
                    icoData.begin() + entries[i].dwImageOffset,
                    icoData.begin() + entries[i].dwImageOffset + entries[i].dwBytesInRes
            );
    
            icons.push_back({grpEntry, iconData});
        }
        return icons;
    }
    
    bool setExeIcon(const std::filesystem::path& exePath,
                         const std::filesystem::path& iconPath,
                         const fs::path& saveFullPath)
    {
        // 1. 读取 ICO 文件
        std::ifstream ifs(iconPath, std::ios::binary | std::ios::ate);
        if (!ifs) {
            return false;
        }
    
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
    
        std::vector<BYTE> icoData(size);
        if (!ifs.read(reinterpret_cast<char*>(icoData.data()), size)) {
            return false;
        }
    
        // 2. 解析 ICO 文件
        auto icons = _extractIconsFromIco(icoData);
        if (icons.empty()) {
            return false;
        }
    
        // 3. 复制原文件到目标路径
        try {
            if (fs::exists(saveFullPath)) {
                fs::remove(saveFullPath);  // 删除已存在的文件
            }
            fs::copy_file(exePath, saveFullPath, fs::copy_options::overwrite_existing);
    
        } catch (const fs::filesystem_error& e) {
            return false;
        }
    
        // 4. 打开目标文件（复制后的文件）
        HANDLE hUpdate = BeginUpdateResourceW(saveFullPath.c_str(), FALSE);
        if (!hUpdate) {
            return false;
        }
    
        // 5. 写入图标组资源
        GRPICONDIR grpDir;
        grpDir.idReserved = 0;
        grpDir.idType = 1;
        grpDir.idCount = static_cast<WORD>(icons.size());
    
        size_t grpDataSize = sizeof(GRPICONDIR) + icons.size() * sizeof(GRPICONDIRENTRY);
        std::vector<BYTE> grpData(grpDataSize);
    
        memcpy(grpData.data(), &grpDir, sizeof(GRPICONDIR));
        memcpy(grpData.data() + sizeof(GRPICONDIR),
               icons.data(),
               icons.size() * sizeof(GRPICONDIRENTRY));
    
        if (!UpdateResourceW(hUpdate, reinterpret_cast<LPCWSTR>(RT_GROUP_ICON), MAKEINTRESOURCEW(1),
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                             grpData.data(), static_cast<DWORD>(grpDataSize))) {
            EndUpdateResourceW(hUpdate, TRUE);
            return false;
        }
    
        // 6. 写入各个图标资源
        for (size_t i = 0; i < icons.size(); i++) {
            WORD iconId = static_cast<WORD>(i + 1);
            auto& [grpEntry, iconData] = icons[i];
    
            if (!UpdateResourceW(hUpdate, reinterpret_cast<LPCWSTR>(RT_ICON), MAKEINTRESOURCEW(iconId),
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                                 iconData.data(), static_cast<DWORD>(iconData.size()))) {
                EndUpdateResourceW(hUpdate, TRUE);
                return false;
            }
        }
    
        // 7. 提交更改
        if (!EndUpdateResourceW(hUpdate, FALSE)) {
            return false;
        }
    
        return true;
    }
    
    
    
    
    /***************** 进程杀死增强版 *****************/
    
    // 启用调试权限（用于终止系统进程）
    static bool __EnableDebugPrivilege__()
    {
        HANDLE hToken;
        TOKEN_PRIVILEGES tkp;
    
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }
    
        LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
        bool result = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL);
        CloseHandle(hToken);
    
        return result;
    }
    
    // 终止单个进程
    static bool _terminateSingleProcess(DWORD pid)
    {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess != NULL) {
            if (TerminateProcess(hProcess, 0)) {
                std::wcout << L"成功终止进程 PID: " << pid << std::endl;
                CloseHandle(hProcess);
                return true;
            }
            CloseHandle(hProcess);
        }
        return false;
    }
    
    // 终止进程树
    static bool _killProcessTree(DWORD pid)
    {
        // 创建新的快照来查找子进程
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return false;
    
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
    
        bool success = true;
    
        // 首先终止所有子进程
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ParentProcessID == pid) {
                    // 递归终止子进程
                    if (!_killProcessTree(pe32.th32ProcessID)) {
                        success = false;
                    }
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
    
        // 然后终止父进程
        success = _terminateSingleProcess(pid) && success;
    
        CloseHandle(hSnapshot);
        return success;
    }
    
    // 增强版进程终止
    bool killProcess(const fs::path& processName, bool killTree, bool partialMatch)
    {
        __EnableDebugPrivilege__();
        int killedCount = 0;
        bool success = false;
    
        // 创建进程快照
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE)
        {
            std::wcerr << L"创建进程快照失败! 错误代码: " << GetLastError() << std::endl;
            return false;
        }
    
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
    
        // 存储匹配的进程ID，用于后续终止进程树
        std::vector<DWORD> targetPids;
    
        // 第一次遍历：查找所有匹配的进程
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                std::wstring currentProcess = pe32.szExeFile;
                bool match = false;
    
                if (partialMatch) {
                    // 部分匹配：检查进程名是否包含目标字符串
                    std::wstring targetName = processName.wstring();
                    std::wstring currentName = currentProcess;
    
                    // 转换为小写进行比较（不区分大小写）
                    std::transform(currentName.begin(), currentName.end(), currentName.begin(), ::tolower);
                    std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
    
                    // 移除.exe后缀进行比较
                    if (currentName.length() > 4 &&
                        currentName.substr(currentName.length() - 4) == L".exe") {
                        currentName = currentName.substr(0, currentName.length() - 4);
                    }
    
                    match = (currentName.find(targetName) != std::wstring::npos);
                } else {
                    // 精确匹配：支持带.exe和不带.exe的匹配
                    std::wstring targetName = processName.wstring();
                    std::wstring currentName = currentProcess;
    
                    // 转换为小写进行比较
                    std::transform(currentName.begin(), currentName.end(), currentName.begin(), ::tolower);
                    std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
    
                    // 检查精确匹配（考虑.exe后缀）
                    match = (currentName == targetName) ||
                            (currentName == targetName + L".exe") ||
                            (currentName + L".exe" == targetName);
                }
    
                if (match) {
                    targetPids.push_back(pe32.th32ProcessID);
                    std::wcout << L"找到匹配进程: " << pe32.szExeFile
                               << L" (PID: " << pe32.th32ProcessID
                               << L", 父PID: " << pe32.th32ParentProcessID << L")" << std::endl;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
    
        // 第二次遍历：终止进程（和可能的进程树）
        for (DWORD pid : targetPids) {
            if (killTree) {
                // 终止整个进程树
                success = _killProcessTree(pid) || success;
            } else {
                // 只终止指定进程
                success = _terminateSingleProcess(pid) || success;
            }
            if (success) killedCount++;
        }
    
        CloseHandle(hSnapshot);
    
        if (killedCount > 0) {
            std::wcout << L"成功终止 " << killedCount << L" 个进程" << std::endl;
        } else {
            std::wcout << L"未找到或无法终止指定进程" << std::endl;
        }
    
        return success;
    }
    
    // 通过PID终止进程
    bool killProcess(DWORD pid, bool killTree)
    {
        __EnableDebugPrivilege__();
        bool success = false;
    
        if (killTree) {
            // 终止整个进程树
            success = _killProcessTree(pid);
        } else {
            // 只终止指定进程
            success = _terminateSingleProcess(pid);
        }
    
        if (success) {
            std::wcout << L"成功终止进程 PID: " << pid << std::endl;
        } else {
            std::wcerr << L"终止进程失败 PID: " << pid
                       << L" 错误代码: " << GetLastError() << std::endl;
        }
    
        return success;
    }
    
    bool killProcess(const fs::path& processName)
    {
        __EnableDebugPrivilege__();
        int killedCount = 0;
    
        // 创建进程快照
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE)
        {
            std::wcerr << L"创建进程快照失败! 错误代码: " << GetLastError() << std::endl;
            return false;
        }
    
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
    
        // 开始遍历进程
        if (!Process32FirstW(hSnapshot, &pe32)) {
            std::wcerr << L"遍历进程失败! 错误代码: " << GetLastError() << std::endl;
            CloseHandle(hSnapshot);
            return false;
        }
    
        do {
            // 不区分大小写比较进程名
            if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                // 打开进程获取句柄
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess != NULL) {
                    // 终止进程
                    if (TerminateProcess(hProcess, 0)) {
                        std::wcout << L"成功杀死进程: " << pe32.szExeFile
                                   << L" (PID: " << pe32.th32ProcessID << L")" << std::endl;
                        killedCount++;
                    }
                    else {
                        std::wcerr << L"终止进程失败! 错误代码: " << GetLastError() << std::endl;
                    }
                    CloseHandle(hProcess);
                }
                else {
                    std::wcerr << L"打开进程失败! 可能需要管理员权限。错误代码: " << GetLastError() << std::endl;
                }
            }
        } while (Process32NextW(hSnapshot, &pe32));
    
        CloseHandle(hSnapshot);
        return true;
    }
    
    
    /**************** 弹窗 ******************/
    WMessageBox::WMessageBox()
    {
        ;
    }
    
    
    void WMessageBox::showInfo(const fs::path& title, const fs::path& content)
    {
        MessageBoxW(nullptr,
                    content.wstring().c_str(),  // 内容
                    title.wstring().c_str(),    // 标题
                    MB_OK | MB_ICONINFORMATION);
    }
    
    void WMessageBox::showWarning(const fs::path& title, const fs::path& content)
    {
        MessageBoxW(nullptr,
                    content.wstring().c_str(),  // 内容
                    title.wstring().c_str(),    // 标题
                    MB_OK | MB_ICONWARNING);
    }
    
    void WMessageBox::showError(const fs::path& title, const fs::path& content)
    {
        MessageBoxW(nullptr,
                    content.wstring().c_str(),  // 内容
                    title.wstring().c_str(),    // 标题
                    MB_OK | MB_ICONERROR);
    }
    
    bool WMessageBox::askYesNo(const fs::path &title, const fs::path &content, const string model)
    {
        int result = IDNO;
    
        if (model == "info")
        {
            result = MessageBoxW(nullptr,
                                 content.wstring().c_str(),  // 内容
                                 title.wstring().c_str(),    // 标题
                                 MB_YESNO | MB_ICONQUESTION);
        }
        else if (model == "warning")
        {
            result = MessageBoxW(nullptr,
                                 content.wstring().c_str(),
                                 title.wstring().c_str(),
                                 MB_YESNO | MB_ICONWARNING);
        }
        else if (model == "error")
        {
            result = MessageBoxW(nullptr,
                                 content.wstring().c_str(),
                                 title.wstring().c_str(),
                                 MB_YESNO | MB_ICONERROR);
        }
        else
        {
            // 默认使用信息图标
            result = MessageBoxW(nullptr,
                                 content.wstring().c_str(),
                                 title.wstring().c_str(),
                                 MB_YESNO | MB_ICONQUESTION);
        }
    
        return (result == IDYES);
    }
    
    // 设置顶层窗口 //
    // 将窗口设置为顶层窗口
    void WindowTop::_setWindowTopMost(bool topMost)
    {
        if (!hwnd)
        {
            cerr << "错误：无效的窗口句柄" << endl;
            return;
        }
    
        // 设置窗口位置标志
        UINT flags = SWP_NOMOVE | SWP_NOSIZE;
    
        if (topMost)
        {
            // 设置为顶层窗口
            SetWindowPos(hwnd, HWND_TOPMOST, px, py, w, h, flags);
        }
        else
        {
            // 取消顶层窗口设置
            SetWindowPos(hwnd, HWND_NOTOPMOST, px, py, w, h, flags);
        }
    }
    
    void WindowTop::setWindowsTopOn()
    {
        _setWindowTopMost(true);
    }
    
    void WindowTop::setWindowsTopOff()
    {
        // 取消顶层窗口设置
        _setWindowTopMost(false);
    }
    
    // 查找窗口句柄的辅助函数
    HWND WindowTop::_findWindowByTitle(const string& windowTitle)
    {
        // 略微延迟 //
        Sleep(200);
    
        // 将ANSI字符串转换为宽字符串
        fs::path wideTitle = windowTitle;
    
        // 查找窗口
        return FindWindowW(NULL, wideTitle.c_str());
    }
    
    WindowTop::WindowTop(const string& hwndName, int pxc, int pyc, int wc, int hc)
    {
        px = pxc;
        py = pyc;
        w = wc;
        h = hc;
        hwnd = _findWindowByTitle(hwndName);
    }
    
    WindowTop::WindowTop(HWND hwnd, int pxc, int pyc, int wc, int hc)
    {
        hwnd = hwnd;
        px = pxc;
        py = pyc;
        w = wc;
        h = hc;
    }
    
    
    // dict继续 //
    
    
    
    /********************** Color类 **********************/
    // 将颜色名称映射到RGB值 //
} // namespace wtl

namespace random
{
    /********************** 随机数 **********************/
    int randInt(int min, int max)
    {
        // 静态变量确保引擎和分布只初始化一次，提升性能
        static std::random_device rd; // 用于生成随机种子
        static std::mt19937 gen(rd()); // 以rd()的返回值初始化随机数引擎
        static std::uniform_int_distribution<int> dis; // 默认构造的分布对象

        // 使用新的范围重新分布，并生成随机数
        return dis(gen, std::uniform_int_distribution<int>::param_type(min, max));
    }

    long randInt(long min, long max)
    {
        // 静态变量确保引擎和分布只初始化一次，提升性能
        static std::random_device rd; // 用于生成随机种子
        static std::mt19937 gen(rd()); // 以rd()的返回值初始化随机数引擎
        static std::uniform_int_distribution<long> dis; // 默认构造的分布对象

        // 使用新的范围重新分布，并生成随机数
        return dis(gen, std::uniform_int_distribution<long>::param_type(min, max));
    }

    long long randInt(long long min, long long max)
    {
        // 静态变量确保引擎和分布只初始化一次，提升性能
        static std::random_device rd; // 用于生成随机种子
        static std::mt19937 gen(rd()); // 以rd()的返回值初始化随机数引擎
        static std::uniform_int_distribution<long long> dis; // 默认构造的分布对象

        // 使用新的范围重新分布，并生成随机数
        return dis(gen, std::uniform_int_distribution<long long>::param_type(min, max));
    }

    // 生成一个在闭区间 [min, max] 内的随机浮点数 //
    float uniform(float min, float max)
    {
        // 使用静态变量确保引擎和分布只初始化一次，提升性能
        static std::random_device rd; // 用于获取真随机种子
        static std::mt19937 gen(rd()); // 使用梅森旋转算法引擎
        static std::uniform_real_distribution<float> dis; // 默认构造的分布对象

        // 使用新的范围重新分布，并生成随机数
        return dis(gen, std::uniform_real_distribution<float>::param_type(min, max));
    }

    // 生成一个在闭区间 [min, max] 内的随机浮点数 //
    double uniform(double min, double max)
    {
        // 使用静态变量确保引擎和分布只初始化一次，提升性能
        static std::random_device rd; // 用于获取真随机种子
        static std::mt19937 gen(rd()); // 使用梅森旋转算法引擎
        static std::uniform_real_distribution<double> dis; // 默认构造的分布对象

        // 使用新的范围重新分布，并生成随机数
        return dis(gen, std::uniform_real_distribution<double>::param_type(min, max));
    }

} // namespace random

/**************** 清理工作 ******************/
static void _cleanupCheck()
{
    /********* 清理未释放的鼠标 *********/
    wtl::MouseEvent::cleanup();

    /********* 清理未释放的键盘 *********/
    wtl::KeyBoardEvent::cleanup();

    // 发送程序结束消息
    PostQuitMessage(0);
    exit(0);
}

namespace wtl
{
    std::vector<unsigned char> getWinRcData(int resourceId)
    {
        // 查找资源
        HRSRC hRes = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hRes) return {};  // 返回空vector

        // 获取资源大小
        DWORD size = SizeofResource(GetModuleHandle(NULL), hRes);
        if (size == 0) return {};

        // 加载资源
        HGLOBAL hData = LoadResource(GetModuleHandle(NULL), hRes);
        if (!hData) return {};

        // 锁定资源获取数据指针
        char* data = static_cast<char*>(LockResource(hData));
        if (!data) return {};

        // 将数据复制到vector并返回
        return std::vector<unsigned char>(data, data + size);
    }

    /**
     * @brief 根据进程的可执行文件路径获取匹配的进程PID
     *
     * @param processPath 进程的可执行文件路径
     * @return DWORD 匹配的进程PID，如果未找到返回0
     */
    unsigned long getProcessPid(const fs::path& processPath)
    {
        // 获取进程路径，支持传入.exe或不带扩展名的文件名
        std::wstring targetName = processPath.filename().wstring();
        std::wstring targetPath = processPath.wstring();

        // 如果传入的是文件名，确保有.exe后缀
        if (processPath.extension().empty()) {
            targetName = targetName + L".exe";
            targetPath = targetPath + L".exe";
        }

        // 创建进程快照
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return 0;
        }

        // 使用智能指针自动释放快照句柄
        auto snapshotGuard = std::unique_ptr<void, decltype(&CloseHandle)>(hSnapshot, CloseHandle);

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        // 遍历所有进程
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                // 比较进程名
                if (_wcsicmp(pe32.szExeFile, targetName.c_str()) == 0) {
                    // 打开进程获取完整路径进行验证
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                                  FALSE, pe32.th32ProcessID);
                    if (hProcess != NULL) {
                        wchar_t processFullPath[MAX_PATH];
                        DWORD bufferSize = MAX_PATH;

                        // 尝试获取进程的可执行文件完整路径
                        if (QueryFullProcessImageNameW(hProcess, 0, processFullPath, &bufferSize)) {
                            fs::path actualPath(processFullPath);

                            // 比较完整路径
                            bool pathMatches = false;

                            // 情况1: 传入的是完整路径
                            if (fs::exists(targetPath)) {
                                pathMatches = fs::equivalent(actualPath, targetPath);
                            }
                                // 情况2: 传入的是文件名或相对路径
                            else {
                                // 比较文件名
                                if (_wcsicmp(actualPath.filename().c_str(), targetName.c_str()) == 0) {
                                    // 如果传入路径是相对路径，尝试解析为完整路径
                                    try {
                                        fs::path fullTargetPath = fs::absolute(targetPath);
                                        if (fs::exists(fullTargetPath)) {
                                            pathMatches = fs::equivalent(actualPath, fullTargetPath);
                                        } else {
                                            // 如果路径不存在，只比较文件名
                                            pathMatches = true;
                                        }
                                    } catch (...) {
                                        // 解析路径失败，只比较文件名
                                        pathMatches = true;
                                    }
                                }
                            }

                            if (pathMatches) {
                                CloseHandle(hProcess);
                                return pe32.th32ProcessID;  // 找到匹配的进程，直接返回PID
                            }
                        } else {
                            // 如果无法获取完整路径，只根据进程名匹配
                            CloseHandle(hProcess);
                            return pe32.th32ProcessID;  // 找到匹配的进程，直接返回PID
                        }

                        CloseHandle(hProcess);
                    } else {
                        // 无法打开进程，只根据进程名匹配
                        return pe32.th32ProcessID;  // 找到匹配的进程，直接返回PID
                    }
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        return 0;  // 未找到匹配的进程
    }

    /**
     * @brief 根据进程的可执行文件路径获取所有匹配的进程PID
     *
     * @param processPath 进程的可执行文件路径
     * @return std::vector<DWORD> 匹配的进程PID列表
     */
    std::vector<DWORD> getProcessPid(const fs::path& processPath,bool getAllPidList)
    {
        std::vector<DWORD> pids;

        // 获取进程路径，支持传入.exe或不带扩展名的文件名
        std::wstring targetName = processPath.filename().wstring();
        std::wstring targetPath = processPath.wstring();

        // 如果传入的是文件名，确保有.exe后缀
        if (processPath.extension().empty()) {
            targetName = targetName + L".exe";
            targetPath = targetPath + L".exe";
        }

        // 创建进程快照
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return pids;
        }

        // 使用智能指针自动释放快照句柄
        auto snapshotGuard = std::unique_ptr<void, decltype(&CloseHandle)>(hSnapshot, CloseHandle);

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        // 遍历所有进程
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                // 比较进程名
                if (_wcsicmp(pe32.szExeFile, targetName.c_str()) == 0) {
                    // 打开进程获取完整路径进行验证
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                                  FALSE, pe32.th32ProcessID);
                    if (hProcess != NULL) {
                        wchar_t processFullPath[MAX_PATH];
                        DWORD bufferSize = MAX_PATH;

                        // 尝试获取进程的可执行文件完整路径
                        if (QueryFullProcessImageNameW(hProcess, 0, processFullPath, &bufferSize)) {
                            fs::path actualPath(processFullPath);

                            // 比较完整路径
                            bool pathMatches = false;

                            // 情况1: 传入的是完整路径
                            if (fs::exists(targetPath)) {
                                pathMatches = fs::equivalent(actualPath, targetPath);
                            }
                                // 情况2: 传入的是文件名或相对路径
                            else {
                                // 比较文件名
                                if (_wcsicmp(actualPath.filename().c_str(), targetName.c_str()) == 0) {
                                    // 如果传入路径是相对路径，尝试解析为完整路径
                                    try {
                                        fs::path fullTargetPath = fs::absolute(targetPath);
                                        if (fs::exists(fullTargetPath)) {
                                            pathMatches = fs::equivalent(actualPath, fullTargetPath);
                                        } else {
                                            // 如果路径不存在，只比较文件名
                                            pathMatches = true;
                                        }
                                    } catch (...) {
                                        // 解析路径失败，只比较文件名
                                        pathMatches = true;
                                    }
                                }
                            }

                            if (pathMatches) {
                                pids.push_back(pe32.th32ProcessID);
                            }
                        } else {
                            // 如果无法获取完整路径，只根据进程名匹配
                            pids.push_back(pe32.th32ProcessID);
                        }

                        CloseHandle(hProcess);
                    } else {
                        // 无法打开进程，只根据进程名匹配
                        pids.push_back(pe32.th32ProcessID);
                    }
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        return pids;
    }


    // 嵌入二进制数据 //
    bool addBinaryResources(const string &exePath,const std::vector<unsigned char>& binary,const string &flag)
    {
        try
        {
            // flag 不能为空，否则无法按区块定位 //
            if (flag.empty())
            {
                println(Color("yellow"), "Error: flag cannot be empty");
                return false;
            }

            // 组装一个区块：魔数 + 版本 + flag长度 + flag + 数据长度 + 数据 //
            std::vector<unsigned char> dataToWrite;

            // 1. 魔数（ASCII，用于定位区块起始） //
            const char magic[] = "WKPBD";
            dataToWrite.insert(dataToWrite.end(), magic, magic + 5);

            // 2. 版本号 //
            dataToWrite.push_back(1);

            // 3. flag 长度（uint32，本机小端） //
            uint32_t flagLen = static_cast<uint32_t>(flag.size());
            const unsigned char* flagLenPtr = reinterpret_cast<const unsigned char*>(&flagLen);
            dataToWrite.insert(dataToWrite.end(), flagLenPtr, flagLenPtr + sizeof(flagLen));

            // 4. flag 字节（UTF-8 原文，中文任意） //
            dataToWrite.insert(dataToWrite.end(), flag.begin(), flag.end());

            // 5. 数据长度（uint64，本机小端） //
            uint64_t dataLen = static_cast<uint64_t>(binary.size());
            const unsigned char* dataLenPtr = reinterpret_cast<const unsigned char*>(&dataLen);
            dataToWrite.insert(dataToWrite.end(), dataLenPtr, dataLenPtr + sizeof(dataLen));

            // 6. 数据本身 //
            dataToWrite.insert(dataToWrite.end(), binary.begin(), binary.end());

            // 7. 以二进制追加模式写入 exe 末尾 //
            Open exeFile(exePath, "ab");
            exeFile.writeBinary(dataToWrite);
            exeFile.close();

            return true;

        } catch (const std::exception& e) {
            println(Color("red"),"Error embedding binary resources: {}", e.what());
            return false;
        }
    }

    // 解析指定 flag 的所有区块 //
    std::vector<std::vector<unsigned char>> parseEmbeddedBinaryDataAll(const std::string &filePath,const string &flag)
    {
        std::vector<std::vector<unsigned char>> results;

        try
        {
            if (flag.empty())
            {
                println(Color("yellow"), "Warning: flag cannot be empty");
                return results;
            }

            // 1. 读取整个文件
            Open file(filePath, "rb");
            auto allBytes = file.readBinary();
            file.close();

            const size_t n = allBytes.size();
            if (n < 10)
                return results;

            const unsigned char magic[5] = { 'W', 'K', 'P', 'B', 'D' };

            size_t pos = 0;
            while (pos + 10 <= n)
            {
                // 匹配魔数 //
                bool isMagic = true;
                for (size_t i = 0; i < 5; ++i)
                {
                    if (allBytes[pos + i] != magic[i])
                    {
                        isMagic = false;
                        break;
                    }
                }

                if (!isMagic)
                {
                    ++pos;
                    continue;
                }

                const size_t blockStart = pos;

                // 版本号（暂不校验，保留兼容） //
                // unsigned char version = allBytes[blockStart + 5];

                // flag 长度 //
                uint32_t flagLen = 0;
                std::memcpy(&flagLen, &allBytes[blockStart + 6], sizeof(flagLen));

                const size_t flagStart = blockStart + 10;
                if (static_cast<size_t>(flagLen) > n - flagStart)
                {
                    pos = blockStart + 1;   // 越界，跳过当前字节继续 //
                    continue;
                }

                // flag 字节 //
                std::string curFlag(allBytes.begin() + flagStart, allBytes.begin() + flagStart + static_cast<size_t>(flagLen));

                const size_t dataLenPos = flagStart + static_cast<size_t>(flagLen);
                if (dataLenPos + sizeof(uint64_t) > n)
                {
                    pos = blockStart + 1;
                    continue;
                }

                uint64_t dataLen = 0;
                std::memcpy(&dataLen, &allBytes[dataLenPos], sizeof(dataLen));

                const size_t dataStart = dataLenPos + sizeof(uint64_t);
                if (static_cast<size_t>(dataLen) > n - dataStart)
                {
                    pos = blockStart + 1;
                    continue;
                }

                const size_t dataEnd = dataStart + static_cast<size_t>(dataLen);

                if (curFlag == flag)
                {
                    results.emplace_back(allBytes.begin() + dataStart, allBytes.begin() + dataEnd);
                }

                // 跳过整个区块继续向后扫描，避免进入数据内部误匹配魔数 //
                pos = dataEnd;
            }

            return results;
        }
        catch (const std::exception& e) {
            println(Color("red"), "Error parsing embedded binary data: {}", e.what());
            return results;
        }
    }

    // 解析指定 flag 的最后一个区块（最新一次嵌入） //
    std::vector<unsigned char> parseEmbeddedBinaryData(const std::string &filePath,const string &flag)
    {
        auto all = parseEmbeddedBinaryDataAll(filePath, flag);
        if (all.empty())
            return {};
        return all.back();
    }


    /***************************************************/

} // namespace wtl

// 清理函数调用 //
void exitCheckWork()
{
    if (!autoClear)
    {
        atexit(_cleanupCheck);  // 退出所执行的任务
        autoClear = true;
    }
}

//int main()
//{
//    wtl::useConsoleUtf8();
//    using namespace wtl;
////    Open ipt("./text.bin", "rb");
////    println(ipt.readBinaryToString());
//    return 0;
//}

namespace wtl
{
    /****************** 调用dll函数 ******************/
    // 初始化调用 //
    CallWinDll::CallWinDll(const fs::path& winDllPath, const std::string &convention)
    {
        callingConvention = convention;
        hDll = LoadLibraryW(winDllPath.wstring().c_str());
        if (hDll == nullptr)
        {
            DWORD error = GetLastError();
            std::cerr << "Failed to load DLL: " << winDllPath.string() << ". Error: " << error << std::endl;
            isOpenFlag = false;
        }
        else
        {
            isOpenFlag = true;
        }
    }

    // 析构函数不变 //
    CallWinDll::~CallWinDll()
    {
        if (hDll != nullptr)
        {
            FreeLibrary(hDll);
            hDll = nullptr;
        }
    }

    void CallWinDll::help()
    {
        const std::string helpText = R"(
    ===================================================
    CallWinDll Library - Dynamic DLL Loader and Function Caller
    ===================================================

    OVERVIEW:
    CallWinDll is a C++ class designed to dynamically load Windows DLLs and call
    their exported functions with support for different calling conventions.

    CONSTRUCTOR:
    CallWinDll(const fs::path& dllPath, const std::string& convention = "stdcall")
      - dllPath: Path to the DLL file (supports Unicode/wide characters)
      - convention: Calling convention (default: "stdcall")
          Valid values: "cdecl", "stdcall", "fastcall", "thiscall"

    MEMBER FUNCTIONS:
    1. bool isOpen() const
       - Returns true if DLL was loaded successfully, false otherwise

    2. void setCallingConvention(const std::string& convention)
       - Change the calling convention for subsequent function calls
       - Convention must be one of: "cdecl", "stdcall", "fastcall", "thiscall"

    3. std::string getCallingConvention() const
       - Returns the current calling convention setting

    4. template<typename ReturnType, typename... Args>
       ReturnType callWinDll(const std::string& funcName, Args&&... args)
       - Dynamically calls a function from the loaded DLL
       - ReturnType: Return type of the DLL function
       - funcName: Name of the exported function (case-sensitive)
       - args: Arguments to pass to the function
       - Returns: The result of the DLL function call
       - Throws: std::runtime_error on failure

    5. void help()
       - Displays this help information

    CALLING CONVENTIONS:
    - "cdecl":     C calling convention, caller cleans stack
    - "stdcall":   Standard Windows API convention, callee cleans stack
    - "fastcall":  Fast calling convention, uses registers for some parameters
    - "thiscall":  Used for C++ member functions (rare for DLL exports)

    USAGE EXAMPLE:
    -------------------------------------------------------------------
    try {
        // Load DLL with stdcall convention (default)
        CallWinDll dll(L"mylibrary.dll");

        if (!dll.isOpen()) {
            std::cerr << "Failed to load DLL!" << std::endl;
            return 1;
        }

        // Call a function that returns int and takes two int parameters
        int result = dll.callWinDll<int>("Add", 5, 3);
        std::cout << "5 + 3 = " << result << std::endl;

        // For DLLs with cdecl convention
        CallWinDll cdeclDll(L"cdecl_lib.dll", "cdecl");
        double val = cdeclDll.callWinDll<double>("Compute", 1.5, 2.5);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    -------------------------------------------------------------------

    ERROR HANDLING:
    - DLL loading failure: Check file path and dependencies
    - Function not found: Verify function name and export status
    - Calling convention mismatch: Ensure correct convention is specified
    - Parameter mismatch: Verify function signature matches the call

    NOTES:
    1. The class uses RAII pattern - DLL is automatically unloaded on destruction
    2. Function names are case-sensitive and must match the exact exported name
    3. For C++ functions, use extern "C" in the DLL to avoid name mangling
    4. All paths support Unicode characters
    5. Exception safety: All methods provide strong exception guarantee

    ===================================================
    )";

        println(Color("yellow"),helpText);
    }
} // namespace wtl
