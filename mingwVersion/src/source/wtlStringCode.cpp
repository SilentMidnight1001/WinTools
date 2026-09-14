#include "wtlStringCode.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <iostream>

#if defined(_MSC_VER) && !defined(WtlMinGW) && !defined(WtlVs)
#define WtlVs
#elif !defined(WtlMinGW) && !defined(WtlVs)
#define WtlMinGW
#endif



/*************** 编码转化 ******************/

// 将字符串编码从UTF8转为GBK编码 //
//string StringCode::gbkToUtf8(const std::string &gbkStr)
//{
//    // 获取宽字符所需的长度
//    int wideCharLen = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, nullptr, 0);
//    if (wideCharLen <= 0) {
//        return ""; // 转换失败
//    }
//
//    // 分配宽字符缓冲区
//    std::wstring wideStr;
//    wideStr.resize(wideCharLen);
//
//    // 将GBK字符串转换为宽字符
//    MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, &wideStr[0], wideCharLen);
//
//    // 获取UTF-8字符串所需的长度
//    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
//    if (utf8Len <= 0) {
//        return ""; // 转换失败
//    }
//
//    // 分配UTF-8字符串缓冲区
//    std::string utf8Str;
//    utf8Str.resize(utf8Len);
//
//    // 将宽字符转换为UTF-8字符串
//    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &utf8Str[0], utf8Len, nullptr, nullptr);
//
//    return utf8Str;
//}

// 主要的编码转换函数
std::string wtl::StringCode::stringChangeEncoding(const std::string &str, const std::string &targetEncoding)
{
    if (str.empty()) {
        return str;
    }

    // 规范化编码名称
    std::string normalizedTargetEncoding = _normalizeEncodingName(targetEncoding);
    std::string srcStr = str;
    std::string detectedEncoding;

    // 移除BOM并检测编码
    srcStr = _removeBom(str, detectedEncoding);

    if (detectedEncoding.empty()) {
        // 如果没有通过BOM检测到编码，使用编码检测函数
        detectedEncoding = checkStringCode(srcStr);
    }

    // 规范化检测到的编码名称
    std::string normalizedSrcEncoding = _normalizeEncodingName(detectedEncoding);

    // 如果源编码和目标编码相同，直接返回
    if (normalizedSrcEncoding == normalizedTargetEncoding) {
        return srcStr;
    }

    // 进行编码转换
    return _convertBetweenEncodings(srcStr, normalizedSrcEncoding, normalizedTargetEncoding);
}

// 辅助函数：规范化编码名称
std::string wtl::StringCode::_normalizeEncodingName(const std::string &encoding)
{
    std::string lowerEncoding = encoding;
    std::transform(lowerEncoding.begin(), lowerEncoding.end(), lowerEncoding.begin(), ::tolower);

    // 处理常见的编码名称变体
    if (lowerEncoding == "utf-8" || lowerEncoding == "utf8") {
        return "utf8";
    } else if (lowerEncoding == "utf-8bom" || lowerEncoding == "utf8bom" || lowerEncoding == "utf-8 with bom") {
        return "utf8bom";
    } else if (lowerEncoding == "utf-16le" || lowerEncoding == "utf16le" || lowerEncoding == "utf-16 le") {
        return "utf16le";
    } else if (lowerEncoding == "utf-16be" || lowerEncoding == "utf16be" || lowerEncoding == "utf-16 be") {
        return "utf16be";
    } else if (lowerEncoding == "utf-16" || lowerEncoding == "utf16") {
        // Windows默认使用UTF-16 LE
#ifdef _WIN32
        return "utf16le";
#else
        return "utf16be";
#endif
    } else if (lowerEncoding == "gb2312" || lowerEncoding == "gbk" || lowerEncoding == "gb18030") {
        // GB2312, GBK, GB18030 都使用GBK转换
        return "gbk";
    } else if (lowerEncoding == "big5") {
        return "big5";
    } else if (lowerEncoding == "latin1" || lowerEncoding == "iso-8859-1") {
        return "latin1";
    } else if (lowerEncoding == "ascii" || lowerEncoding == "us-ascii") {
        return "ascii";
    }

    return lowerEncoding;
}

// 辅助函数：检查是否为UTF-8系列编码
bool wtl::StringCode::_isUtf8Encoding(const std::string &encoding)
{
    return encoding == "utf8" || encoding == "utf8bom";
}

// 辅助函数：检查是否为UTF-16系列编码
bool wtl::StringCode::_isUtf16Encoding(const std::string &encoding)
{
    return encoding == "utf16le" || encoding == "utf16be" || encoding == "utf16";
}

// 辅助函数：检查是否为UTF-32系列编码
bool wtl::StringCode::_isUtf32Encoding(const std::string &encoding)
{
    return encoding == "utf32le" || encoding == "utf32be" || encoding == "utf32";
}

// 辅助函数：移除BOM标记并检测编码
std::string wtl::StringCode::_removeBom(const std::string &str, std::string &detectedEncoding)
{
    detectedEncoding.clear();

    if (str.empty()) {
        return str;
    }

    const unsigned char* data = reinterpret_cast<const unsigned char*>(str.c_str());
    size_t len = str.length();

    // 检查BOM标记
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        detectedEncoding = "utf8bom";
        return str.substr(3);  // 移除UTF-8 BOM
    }
    if (len >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        detectedEncoding = "utf16le";
        return str.substr(2);  // 移除UTF-16 LE BOM
    }
    if (len >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        detectedEncoding = "utf16be";
        return str.substr(2);  // 移除UTF-16 BE BOM
    }
    if (len >= 4 && data[0] == 0xFF && data[1] == 0xFE && data[2] == 0x00 && data[3] == 0x00) {
        detectedEncoding = "utf32le";
        return str.substr(4);  // 移除UTF-32 LE BOM
    }
    if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0xFE && data[3] == 0xFF) {
        detectedEncoding = "utf32be";
        return str.substr(4);  // 移除UTF-32 BE BOM
    }

    return str;  // 没有BOM，返回原字符串
}

// 辅助函数：将任何编码转换为UTF-8
std::string wtl::StringCode::_convertToUtf8(const std::string &str, const std::string &fromEncoding)
{
    if (str.empty()) {
        return str;
    }

    if (fromEncoding == "utf8" || fromEncoding == "utf8bom") {
        // 已经是UTF-8
        return str;
    } else if (fromEncoding == "ascii") {
        // ASCII可以直接转换为UTF-8（实际上是相同的）
        return str;
    } else if (fromEncoding == "gbk") {
        // GBK转UTF-8
        return gbkToUtf8(str);
    } else if (fromEncoding == "big5") {
        // Big5转UTF-8（通过宽字符中转）
        // 注意：这里需要实现Big5到UTF-8的转换
        // 由于没有现成的Big5转换函数，我们通过宽字符中转
        std::wstring wstr = strToWStr(str, true);  // 假设使用GBK转换（Big5和GBK在某些Windows环境下可能混用）
        std::string result = wStrToStr(wstr);
        return result;
    } else if (fromEncoding == "latin1") {
        // Latin1/ISO-8859-1转UTF-8
        std::wstring wstr;
        wstr.resize(str.size());

        for (size_t i = 0; i < str.size(); ++i) {
            wstr[i] = static_cast<unsigned char>(str[i]);
        }

        return wStrToStr(wstr);
    } else if (fromEncoding == "utf16le") {
        // UTF-16 LE转UTF-8
        if (str.size() % 2 != 0) {
            // 不是有效的UTF-16字符串
            return str;
        }

        // 将UTF-16 LE字节转换为wstring
        std::wstring wstr;
        wstr.resize(str.size() / 2);
        memcpy(&wstr[0], str.data(), str.size());

        return wStrToStr(wstr);
    } else if (fromEncoding == "utf16be") {
        // UTF-16 BE转UTF-8（需要字节交换）
        if (str.size() % 2 != 0) {
            return str;
        }

        std::wstring wstr;
        wstr.resize(str.size() / 2);
        const unsigned char* data = reinterpret_cast<const unsigned char*>(str.c_str());

        for (size_t i = 0; i < str.size(); i += 2) {
            wchar_t ch = (data[i] << 8) | data[i + 1];  // 字节交换
            wstr[i / 2] = ch;
        }

        return wStrToStr(wstr);
    }

    // 未知编码，尝试直接返回
    return str;
}

// 辅助函数：从UTF-8转换为目标编码
std::string wtl::StringCode::_convertFromUtf8(const std::string &str, const std::string &toEncoding)
{
    if (str.empty()) {
        return str;
    }

    if (toEncoding == "utf8") {
        // 已经是UTF-8，没有BOM
        return str;
    } else if (toEncoding == "utf8bom") {
        // UTF-8 with BOM
        std::string result = "\xEF\xBB\xBF";
        result += str;
        return result;
    } else if (toEncoding == "ascii") {
        // UTF-8转ASCII（可能会丢失信息）
        std::string result;
        result.reserve(str.size());

        for (size_t i = 0; i < str.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(str[i]);
            if (c < 0x80) {
                result.push_back(str[i]);
            } else if ((c & 0xE0) == 0xC0) {
                // 2字节UTF-8字符，检查是否可以转换为ASCII
                if (i + 1 < str.size()) {
                    // 对于UTF-8转ASCII，我们只保留ASCII字符
                    // 非ASCII字符可以用?代替
                    result.push_back('?');
                }
                ++i;  // 跳过下一个字节
            } else if ((c & 0xF0) == 0xE0) {
                // 3字节UTF-8字符
                if (i + 2 < str.size()) {
                    result.push_back('?');
                }
                i += 2;  // 跳过两个字节
            } else if ((c & 0xF8) == 0xF0) {
                // 4字节UTF-8字符
                if (i + 3 < str.size()) {
                    result.push_back('?');
                }
                i += 3;  // 跳过三个字节
            } else {
                result.push_back('?');
            }
        }

        return result;
    } else if (toEncoding == "gbk") {
        // UTF-8转GBK
        return utf8ToGbk(str);
    } else if (toEncoding == "big5") {
        // UTF-8转Big5（通过宽字符中转）
        std::wstring wstr = strToWStr(str);
        // 注意：这里需要实现UTF-8到Big5的转换
        // 由于没有现成的转换函数，我们返回原始字符串
        return str;
    } else if (toEncoding == "latin1") {
        // UTF-8转Latin1（可能会丢失信息）
        std::wstring wstr = strToWStr(str);
        std::string result;
        result.reserve(wstr.size());

        for (size_t i = 0; i < wstr.size(); ++i) {
            wchar_t wc = wstr[i];
            if (wc < 256) {
                result.push_back(static_cast<char>(wc));
            } else {
                result.push_back('?');  // 替换无法表示的字符
            }
        }

        return result;
    } else if (toEncoding == "utf16le") {
        // UTF-8转UTF-16 LE
        std::wstring wstr = strToWStr(str);
        std::string result;
        result.resize(wstr.size() * 2);

        for (size_t i = 0; i < wstr.size(); ++i) {
            wchar_t wc = wstr[i];
            result[i * 2] = static_cast<char>(wc & 0xFF);
            result[i * 2 + 1] = static_cast<char>((wc >> 8) & 0xFF);
        }

        return result;
    } else if (toEncoding == "utf16be") {
        // UTF-8转UTF-16 BE
        std::wstring wstr = strToWStr(str);
        std::string result;
        result.resize(wstr.size() * 2);

        for (size_t i = 0; i < wstr.size(); ++i) {
            wchar_t wc = wstr[i];
            result[i * 2] = static_cast<char>((wc >> 8) & 0xFF);
            result[i * 2 + 1] = static_cast<char>(wc & 0xFF);
        }

        return result;
    } else if (toEncoding == "utf16") {
        // UTF-8转UTF-16（带BOM）
#ifdef _WIN32
        std::wstring wstr = strToWStr(str);
        std::string result = "\xFF\xFE";  // UTF-16 LE BOM
        result.resize(2 + wstr.size() * 2);

        for (size_t i = 0; i < wstr.size(); ++i) {
            wchar_t wc = wstr[i];
            result[2 + i * 2] = static_cast<char>(wc & 0xFF);
            result[2 + i * 2 + 1] = static_cast<char>((wc >> 8) & 0xFF);
        }

        return result;
#else
        std::wstring wstr = strToWStr(str);
        std::string result = "\xFE\xFF";  // UTF-16 BE BOM
        result.resize(2 + wstr.size() * 2);

        for (size_t i = 0; i < wstr.size(); ++i) {
            wchar_t wc = wstr[i];
            result[2 + i * 2] = static_cast<char>((wc >> 8) & 0xFF);
            result[2 + i * 2 + 1] = static_cast<char>(wc & 0xFF);
        }

        return result;
#endif
    }

    // 未知目标编码，返回原始字符串
    return str;
}

// 辅助函数：在任意两种编码之间转换
std::string wtl::StringCode::_convertBetweenEncodings(const std::string &str, const std::string &fromEncoding, const std::string &toEncoding)
{
    // 如果源编码和目标编码相同，直接返回
    if (fromEncoding == toEncoding) {
        return str;
    }

    // 统一转换流程：源编码 -> UTF-8 -> 目标编码
    std::string utf8Str = _convertToUtf8(str, fromEncoding);
    return _convertFromUtf8(utf8Str, toEncoding);
}


string wtl::StringCode::gbkToUtf8(const std::string& gbkStr)
{
    int wlen = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return "";

    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, &wstr[0], wlen);

    // 宽字符 → UTF-8
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return "";

    std::string utf8(ulen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], ulen, nullptr, nullptr);

    // 移除多余的终止符（API 自动添加的）
    if (!utf8.empty() && utf8.back() == '\0') {
        utf8.pop_back();
    }
    return utf8;
}


string wtl::StringCode::utf8ToGbk(const std::string& utf8Str)
{
    // 获取转换后所需缓冲区大小
    int wideCharLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    if (wideCharLen == 0) return "";

    // 转换为宽字符（UTF-16）
    wchar_t* wideCharBuffer = new wchar_t[wideCharLen];
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wideCharBuffer, wideCharLen) == 0) {
        delete[] wideCharBuffer;
        return "";
    }

    // 获取目标GBK缓冲区大小
    int gbkLen = WideCharToMultiByte(CP_ACP, 0, wideCharBuffer, -1, nullptr, 0, nullptr, nullptr);
    if (gbkLen == 0) {
        delete[] wideCharBuffer;
        return "";
    }

    // 转换为GBK
    char* gbkBuffer = new char[gbkLen];
    if (WideCharToMultiByte(CP_ACP, 0, wideCharBuffer, -1, gbkBuffer, gbkLen, nullptr, nullptr) == 0) {
        delete[] wideCharBuffer;
        delete[] gbkBuffer;
        return "";
    }

    std::string gbkStr(gbkBuffer);

    // 清理内存
    delete[] wideCharBuffer;
    delete[] gbkBuffer;

    return gbkStr;
}


//wstring StringCode::strToWStr(const std::string &str)
//{
//    std::wstring wstr(str.size(), 0);
//    std::transform(str.begin(), str.end(), wstr.begin(),
//                   [](char c) { return static_cast<wchar_t>(c); }
//    );
//    return wstr;
//}

wstring wtl::StringCode::strToWStr(const std::string& str)
{
    // 添加错误处理，默认使用系统ANSI编码
    return strToWStr(str, false); // 默认不使用UTF-8
}

#ifdef WtlMinGW
wstring wtl::StringCode::strToWStr(const std::string& str, bool useGBK)
{
    if (str.empty())
        return L"";

    // 确定源编码：UTF-8或当前系统ANSI编码（GBK）
    UINT codePage = useGBK ? CP_UTF8 : CP_ACP;

    // 计算所需宽字符数
    int wcharCount = MultiByteToWideChar(
            codePage,     // 源编码类型
            0,            // 标志位（无特殊处理）
            str.c_str(),  // 源字符串指针
            -1,           // 自动确定长度（包含终止符）
            nullptr,      // 目标缓冲区（为空时计算长度）
            0
    );
    if (wcharCount == 0)
        return L"";

    // 分配缓冲区并执行转换
    std::wstring wstr;
    wstr.resize(wcharCount);  // 包含终止符空间
    MultiByteToWideChar(
            codePage,
            0,
            str.c_str(),
            -1,
            &wstr[0],
            wcharCount
    );

    wstr.pop_back();  // 移除转换后多余的终止符(L'\0')
    return wstr;
}
#elif defined(WtlVs)
wstring wtl::StringCode::strToWStr(const std::string& str, bool useGBK)
{
    if (str.empty())
        return L"";

    // 确定源编码：UTF-8或当前系统ANSI编码（GBK）
    UINT codePage = useGBK ? CP_UTF8 : CP_ACP;

    // 计算所需宽字符数
    int wcharCount = MultiByteToWideChar(
        codePage,
        MB_ERR_INVALID_CHARS,  // 添加错误检测标志
        str.c_str(),
        -1,
        nullptr,
        0
    );

    if (wcharCount == 0)
    {
        DWORD error = GetLastError();
        // VS调试输出
#ifdef DEBUG
        std::cerr << "MultiByteToWideChar failed. Error code: " << error << std::endl;
#endif
        return L"";
    }

    // 分配缓冲区并执行转换
    std::wstring wstr;
    wstr.resize(wcharCount);

    if (MultiByteToWideChar(
        codePage,
        0,
        str.c_str(),
        -1,
        &wstr[0],
        wcharCount
    ) == 0)
    {
#ifdef DEBUG
        std::cerr << "MultiByteToWideChar conversion failed." << std::endl;
#endif
        return L"";
    }

    // 移除转换后多余的终止符(L'\0')
    if (!wstr.empty() && wstr.back() == L'\0') {
        wstr.pop_back();
    }

    return wstr;
}
#endif

#ifdef WtlMinGW
string wtl::StringCode::wStrToStr(const std::wstring& wstr)
{
    if (wstr.empty()) return "";

    // 计算所需的缓冲区大小（不包括终止符）
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";

    std::string str(len, 0);

    // 执行转换，指定字符串长度而不是使用-1（避免自动添加终止符）
    int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], len, NULL, NULL);
    if (result == 0) return "";

    return str;
}
#elif defined(WtlVs)
string wtl::StringCode::wStrToStr(const std::wstring& wstr)
{
    if (wstr.empty())
        return "";

    // 计算所需的缓冲区大小（不包括终止符）
    int len = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(),
        NULL,
        0,
        NULL,
        NULL
    );

    if (len <= 0)
    {
#ifdef _DEBUG
        std::cerr << "WideCharToMultiByte size calculation failed." << std::endl;
#endif
        return "";
    }

    std::string str(len, 0);

    // 执行转换，指定字符串长度而不是使用-1
    if (WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(),
        &str[0],
        len,
        NULL,
        NULL
    ) == 0)
    {
#ifdef _DEBUG
        std::cerr << "WideCharToMultiByte conversion failed." << std::endl;
#endif
        return "";
    }

    return str;
}
#endif

std::string wtl::StringCode::checkStringCode(const std::string &str)
{
    if (str.empty())
    {
        return "";  // 空字符返回空
    }

    const unsigned char* data = reinterpret_cast<const unsigned char*>(str.c_str());
    size_t len = str.length();

    // 检查BOM标记
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return "utf8bom";
    }
    if (len >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        return "utf16le";
    }
    if (len >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        return "utf16be";
    }
    if (len >= 4 && data[0] == 0xFF && data[1] == 0xFE && data[2] == 0x00 && data[3] == 0x00) {
        return "utf32le";
    }
    if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0xFE && data[3] == 0xFF) {
        return "utf32be";
    }

    // 检测UTF-8
    bool isUtf8 = true;
    size_t i = 0;
    int utf8Chars = 0;
    int asciiChars = 0;

    while (i < len) {
        unsigned char c = data[i];

        if (c < 0x80) {
            // ASCII字符
            asciiChars++;
            i++;
        }
        else if ((c & 0xE0) == 0xC0) {  // 2字节UTF-8
            if (i + 1 >= len || (data[i+1] & 0xC0) != 0x80) {
                isUtf8 = false;
                break;
            }
            utf8Chars++;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0) {  // 3字节UTF-8
            if (i + 2 >= len || (data[i+1] & 0xC0) != 0x80 || (data[i+2] & 0xC0) != 0x80) {
                isUtf8 = false;
                break;
            }
            utf8Chars++;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0) {  // 4字节UTF-8
            if (i + 3 >= len || (data[i+1] & 0xC0) != 0x80 ||
                (data[i+2] & 0xC0) != 0x80 || (data[i+3] & 0xC0) != 0x80) {
                isUtf8 = false;
                break;
            }
            utf8Chars++;
            i += 4;
        }
        else {
            isUtf8 = false;
            break;
        }
    }

    if (isUtf8 && i == len) {
        // 如果是纯ASCII，返回ascii，否则返回utf8
        return (utf8Chars == 0) ? "ascii" : "utf8";
    }

    // 检测GBK编码
    // GBK编码规则：高位字节0x81-0xFE，低位字节0x40-0xFE（除0x7F）
    bool isGbk = true;
    int gbkChars = 0;

    for (i = 0; i < len; ) {
        unsigned char c = data[i];

        if (c < 0x80) {
            // ASCII字符
            i++;
        }
        else if (c >= 0x81 && c <= 0xFE) {
            // 可能是GBK高位字节
            if (i + 1 >= len) {
                isGbk = false;
                break;
            }

            unsigned char c2 = data[i + 1];
            // GBK低位字节范围
            if ((c2 >= 0x40 && c2 <= 0x7E) || (c2 >= 0x80 && c2 <= 0xFE)) {
                gbkChars++;
                i += 2;
            } else {
                isGbk = false;
                break;
            }
        }
        else {
            isGbk = false;
            break;
        }
    }

    if (isGbk && gbkChars > 0) {
        return "gbk";
    }

    // 检测GB2312（GBK的子集）
    bool isGb2312 = true;
    int gb2312Chars = 0;

    for (i = 0; i < len; ) {
        unsigned char c = data[i];

        if (c < 0x80) {
            i++;
        }
        else if (c >= 0xA1 && c <= 0xF7) {  // GB2312高位字节
            if (i + 1 >= len) {
                isGb2312 = false;
                break;
            }

            unsigned char c2 = data[i + 1];
            // GB2312低位字节
            if (c2 >= 0xA1 && c2 <= 0xFE) {
                gb2312Chars++;
                i += 2;
            } else {
                isGb2312 = false;
                break;
            }
        }
        else {
            isGb2312 = false;
            break;
        }
    }

    if (isGb2312 && gb2312Chars > 0) {
        return "gb2312";
    }

    // 检测Big5（繁体中文）
    bool isBig5 = true;
    int big5Chars = 0;

    for (i = 0; i < len; ) {
        unsigned char c = data[i];

        if (c < 0x80) {
            i++;
        }
        else if (c >= 0xA1 && c <= 0xFE) {  // Big5高位字节
            if (i + 1 >= len) {
                isBig5 = false;
                break;
            }

            unsigned char c2 = data[i + 1];
            // Big5低位字节范围
            if ((c2 >= 0x40 && c2 <= 0x7E) || (c2 >= 0xA1 && c2 <= 0xFE)) {
                big5Chars++;
                i += 2;
            } else {
                isBig5 = false;
                break;
            }
        }
        else {
            isBig5 = false;
            break;
        }
    }

    if (isBig5 && big5Chars > 0) {
        return "big5";
    }

    // 检测Latin-1/ISO-8859-1
    bool isLatin1 = true;
    for (i = 0; i < len; i++) {
        if (data[i] >= 0x80 && data[i] < 0xA0) {
            isLatin1 = false;
            break;
        }
    }
    if (isLatin1) {
        return "latin1";
    }

    // 检测UTF-16
    if (len % 2 == 0) {
        int zeroCount = 0;
        for (i = 0; i < len; i++) {
            if (data[i] == 0) {
                zeroCount++;
            }
        }

        if (zeroCount > len / 3) {  // 较多零字节
            // 判断字节序
            bool maybeLE = true;
            bool maybeBE = true;

            for (i = 0; i < len; i += 2) {
                if (data[i] == 0 && data[i + 1] != 0) {
                    maybeBE = false;
                }
                if (data[i] != 0 && data[i + 1] == 0) {
                    maybeLE = false;
                }
            }

            if (maybeLE) return "utf16le";
            if (maybeBE) return "utf16be";
        }
    }

    // 检查是否为纯ASCII
    bool isAscii = true;
    for (i = 0; i < len; i++) {
        if (data[i] >= 0x80) {
            isAscii = false;
            break;
        }
    }

    if (isAscii) {
        return "ascii";
    }

    // 如果以上都不匹配，尝试通用检测
    // 检查是否为可能的8位编码
    bool hasHighBytes = false;
    for (i = 0; i < len; i++) {
        if (data[i] > 0x7F) {
            hasHighBytes = true;
            break;
        }
    }

    if (hasHighBytes)
    {
        // 如果是Windows中文环境，默认猜测是GBK
#ifdef _WIN32
        return "gbk";
#else
        return "utf8";
#endif
    }

    return "binary/other";
}
