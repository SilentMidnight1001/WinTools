#include <shobjidl.h>
#include "winToolsH.hpp"

// ============================================================
// _filedialog — 辅助命名空间（以 _ 开头表明为内部实现）
// Qt 风格 filter → WinAPI 格式转换
// 编码转换统一使用 std::filesystem::path（C++17 内置 UTF-8 ↔ 宽字符）
// ============================================================
namespace _filedialog
{

    // WinAPI OPENFILENAME.lpstrFilter 要求的格式:
    //   L"描述\0模式\0描述\0模式\0\0"（双 null 结尾）

    // 主重载: Qt 风格 filter 字符串 → WinAPI 格式
    // Qt: "Images (*.png *.jpg);;Text files (*.txt)"
    //  →  L"Images (*.png *.jpg)\0*.png;*.jpg\0Text files (*.txt)\0*.txt\0\0"
    static inline std::wstring to_win32_filter(const std::string& qt_filter) {
        if (qt_filter.empty()) return L"";

        // 按 ";;" 分割各过滤器
        std::vector<std::string> sections;
        size_t prev = 0;
        const std::string delim = ";;";
        size_t pos;
        while ((pos = qt_filter.find(delim, prev)) != std::string::npos) {
            sections.push_back(qt_filter.substr(prev, pos - prev));
            prev = pos + 2;
        }
        sections.push_back(qt_filter.substr(prev));

        std::wstring result;
        for (const auto& sec : sections) {
            if (sec.empty()) continue;

            std::string desc = sec;
            std::string raw_pattern;

            // 提取括号中的模式: "Description (*.ext1 *.ext2)"
            auto left = sec.rfind('(');
            auto right = sec.rfind(')');
            if (left != std::string::npos && right != std::string::npos && right > left) {
                raw_pattern = sec.substr(left + 1, right - left - 1);
                // desc 保留含括号的完整描述，与 Qt 行为一致
            } else
            {
                // 没有括号: 整体当作模式
                raw_pattern = sec;
            }

            // 空格分隔的模式 → 分号分隔（WinAPI 要求） //
            // "*.png *.jpg" → "*.png;*.jpg" //
            std::string win_pattern;
            std::istringstream pat_stream(raw_pattern);
            std::string ext;
            bool first = true;
            while (pat_stream >> ext) {
                if (!first) win_pattern += ';';
                win_pattern += ext;
                first = false;
            }
            if (win_pattern.empty()) win_pattern = "*.*";

            //  fs::path 完成 UTF-8 → wstring，无需手动 MultiByteToWideChar //
            result += std::filesystem::path(desc).wstring();
            result += L'\0';
            result += std::filesystem::path(win_pattern).wstring();
            result += L'\0';
        }
        result += L'\0'; // WinAPI 双 null 终止
        return result;
    }

}



// ============================================================================ //
//  Implementation — askOpenFilename
// ============================================================================ //

string filedialog::askOpenFilename(
        const string &caption,
        const string &dir,
        const string &filter)
{
    std::wstring wtitle = caption.empty()
                          ? L"选择文件"   // "选择文件"
                          : fs::path(caption).wstring();
    std::wstring wdir = fs::path(dir).wstring();

    // 预分配缓冲区 — 之前是空指针 CHAR* file{}，导致 GetOpenFileName 失败
    wchar_t file[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;

    std::wstring win_filter = _filedialog::to_win32_filter(filter);
    ofn.lpstrFilter = win_filter.empty() ? nullptr : win_filter.c_str();

    ofn.lpstrTitle = wtitle.c_str();
    ofn.lpstrInitialDir = wdir.empty() ? nullptr : wdir.c_str();

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
                | OFN_ENABLESIZING
                | OFN_HIDEREADONLY
                | OFN_NOCHANGEDIR;

    return GetOpenFileNameW(&ofn)
           ? fs::path(file).generic_string()
           : "";
}


// ============================================================================
//  Implementation — askOpenFilenames
// ============================================================================

vector<string> filedialog::askOpenFilenames(
        const string &caption,
        const string &dir,
        const string &filter)
{
    std::wstring wtitle = caption.empty()
                          ? L"选择文件"
                          : fs::path(caption).wstring();
    std::wstring wdir = fs::path(dir).wstring();

    constexpr size_t BUF_SIZE = 8192;
    wchar_t file[BUF_SIZE]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = file;
    ofn.nMaxFile = BUF_SIZE;

    std::wstring win_filter = _filedialog::to_win32_filter(filter);
    ofn.lpstrFilter = win_filter.empty() ? nullptr : win_filter.c_str();

    ofn.lpstrTitle = wtitle.c_str();
    ofn.lpstrInitialDir = wdir.empty() ? nullptr : wdir.c_str();

    ofn.Flags = OFN_PATHMUSTEXIST
                | OFN_EXPLORER
                | OFN_ENABLESIZING
                | OFN_HIDEREADONLY
                | OFN_ALLOWMULTISELECT
                | OFN_NOCHANGEDIR;

    vector<string> result;

    if (!GetOpenFileNameW(&ofn))
        return result;

    // Parse multi-select: directory \0 file1 \0 file2 \0 \0
    const wchar_t* p = file;
    std::wstring dirPath(p);
    p += dirPath.size() + 1;

    if (*p == L'\0') {
        // Single file selected
        result.push_back(fs::path(dirPath).generic_string());
        return result;
    }

    while (*p) {
        std::wstring fname(p);
        result.push_back(
                fs::path(dirPath + L"\\" + fname).generic_string());
        p += fname.size() + 1;
    }

    return result;
}


// ============================================================================
//  Implementation — askDirectory (IFileOpenDialog with FOS_PICKFOLDERS)
// ============================================================================

string filedialog::askDirectory(
        const string &caption,
        const string &dir)
{
    std::wstring wtitle = caption.empty()
                          ? L"选择文件夹"     // "选择文件夹"
                          : fs::path(caption).wstring();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comOwner = (hr == S_OK);   // only uninit if WE initialised COM

    // 保存当前工作目录，因为 IFileOpenDialog 会改变它
    std::string savedCwd = fs::current_path().string();

    IFileOpenDialog *pDlg = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&pDlg));
    if (FAILED(hr)) {
        if (comOwner) CoUninitialize();
        return "";
    }

    // FOS_PICKFOLDERS → folder selection mode
    DWORD opts;
    pDlg->GetOptions(&opts);
    pDlg->SetOptions(opts | FOS_PICKFOLDERS);
    pDlg->SetTitle(wtitle.c_str());

    // Optional: set initial folder
    std::wstring wdir = fs::path(dir).wstring();
    if (!wdir.empty()) {
        IShellItem *pInit = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(
                wdir.c_str(), nullptr, IID_PPV_ARGS(&pInit)))) {
            pDlg->SetFolder(pInit);
            pInit->Release();
        }
    }

    string result;
    hr = pDlg->Show(nullptr);

    // 恢复工作目录 — IFileOpenDialog 会改变当前目录
    std::error_code ec;
    fs::current_path(savedCwd, ec);

    if (SUCCEEDED(hr)) {
        IShellItem *pItem = nullptr;
        if (SUCCEEDED(pDlg->GetResult(&pItem))) {
            PWSTR pPath = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(
                    SIGDN_FILESYSPATH, &pPath))) {
                result = fs::path(std::wstring(pPath)).generic_string();
                CoTaskMemFree(pPath);
            }
            pItem->Release();
        }
    }

    pDlg->Release();
    if (comOwner) CoUninitialize();
    return result;
}
