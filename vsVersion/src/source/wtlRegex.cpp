#include "wtlRegex.hpp"
#include "wtlStringCode.hpp"
#include <iostream>
#include <regex>
#include <string>

#if defined(_MSC_VER) && !defined(WtlMinGW) && !defined(WtlVs)
#define WtlVs
#elif !defined(WtlMinGW) && !defined(WtlVs)
#define WtlMinGW
#endif

/************************* 正则表达式 *************************/

// 转义正则元字符，使 pattern 按字面文本匹配（类似 Python re.escape）
string re::escape(const string &pattern)
{
    string result;
    result.reserve(pattern.size());

    for (size_t i = 0; i < pattern.size(); ++i)
    {
        const char c = pattern[i];

        // 非 ASCII 字节保持原样，避免破坏 UTF-8 中文路径等内容 //
        if (static_cast<unsigned char>(c) >= 0x80)
        {
            result += c;
            continue;
        }

        switch (c)
        {
            case '^':
            case '$':
            case '\\':
            case '.':
            case '*':
            case '+':
            case '?':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
                result.push_back('\\');
                result.push_back(c);
                break;
            default:
                result += c;
                break;
        }
    }

    return result;
}

/************************* 正则表达式 sub 函数 *************************/
#ifdef WtlMinGW
// 在 re 命名空间中修改 findAll 函数
vector<string> re::findAll(const string &pattern, const string &text, int flags)
{
    vector<string> results;
    wtl::StringCode scd;

    try {
        // 将UTF-8字符串转换为宽字符串
        std::wstring wideText = scd.strToWStr(text, true);
        std::wstring widePattern = scd.strToWStr(pattern, true);

        // 处理flags
        std::regex_constants::syntax_option_type regex_flags = std::regex_constants::ECMAScript;
        if (flags & IGNORECASE)
        {
            regex_flags |= std::regex_constants::icase;
        }

        if (flags & MULTILINE)
        {
            // C++中multiline模式需要特殊处理
            regex_flags |= std::regex_constants::multiline;
        }

        // 使用宽字符正则表达式
        std::wregex re(widePattern, regex_flags);

        // 使用迭代器进行匹配
        auto words_begin = std::wsregex_iterator(wideText.begin(), wideText.end(), re);
        auto words_end = std::wsregex_iterator();

        // 检查模式中是否有捕获组（括号）
        bool has_capture_groups = (pattern.find('(') != string::npos &&
                                   pattern.find(')') != string::npos);

        // 遍历所有匹配结果
        for (std::wsregex_iterator i = words_begin; i != words_end; ++i)
        {
            std::wsmatch match = *i;

            if (has_capture_groups && match.size() > 1)
            {
                // 有捕获组：返回所有捕获组的内容（类似于Python行为）
                for (size_t j = 1; j < match.size(); ++j)
                {
                    if (match[j].matched)
                    {
                        std::string utf8_result = scd.wStrToStr(match[j].str());
                        results.push_back(utf8_result);
                    }
                }
            }
            else
            {
                // 没有捕获组：返回整个匹配
                std::string utf8_result = scd.wStrToStr(match.str());
                results.push_back(utf8_result);
            }
        }

    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return results;
}

// 同时修改 search 函数以支持捕获组 //
string re::search(const string &pattern, const string &text, int flags)
{
    wtl::StringCode scd;

    try {
        std::wstring wideText = scd.strToWStr(text, true);
        std::wstring widePattern = scd.strToWStr(pattern, true);

        std::regex_constants::syntax_option_type regex_flags = std::regex_constants::ECMAScript;

        if (flags & IGNORECASE) {
            regex_flags |= std::regex_constants::icase;
        }

        if (flags & MULTILINE) {
            regex_flags |= std::regex_constants::multiline;
        }

        std::wregex re(widePattern, regex_flags);
        std::wsmatch match;

        if (std::regex_search(wideText, match, re)) {
            // 如果有捕获组，返回第一个捕获组的内容
            // 如果没有捕获组，返回整个匹配
            if (match.size() > 1) {
                return scd.wStrToStr(match[1].str()); // 返回第一个捕获组
            } else {
                return scd.wStrToStr(match.str()); // 返回整个匹配
            }
        }

    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return "";
}

string re::sub(const string& pattern, const string& repl, const string& text, int flags)
{
    wtl::StringCode scd;
    string result = text;

    try {
        // 将UTF-8字符串转换为宽字符串
        std::wstring wideText = scd.strToWStr(text, true);
        std::wstring widePattern = scd.strToWStr(pattern, true);
        std::wstring wideRepl = scd.strToWStr(repl, true);

        // 处理flags
        std::regex_constants::syntax_option_type regex_flags = std::regex_constants::ECMAScript;
        if (flags & IGNORECASE) {
            regex_flags |= std::regex_constants::icase;
        }
        if (flags & MULTILINE) {
            regex_flags |= std::regex_constants::multiline;
        }

        // 创建宽字符正则表达式
        std::wregex re(widePattern, regex_flags);

        // 执行替换操作
        std::wstring wideResult = std::regex_replace(wideText, re, wideRepl);

        // 转换回UTF-8
        result = scd.wStrToStr(wideResult);

    }
    catch (const std::regex_error& e) {
        std::cerr << "Regex error in sub: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in sub: " << e.what() << std::endl;
    }

    return result;
}

string re::sub(const std::string& pattern, std::function<string(const std::smatch&)> callback,
               const std::string& text, int flags)
{
    wtl::StringCode scd;
    string result = text;

    try {
        std::wstring wideText = scd.strToWStr(text, true);
        std::wstring widePattern = scd.strToWStr(pattern, true);

        std::regex_constants::syntax_option_type regex_flags = std::regex_constants::ECMAScript;
        if (flags & IGNORECASE) {
            regex_flags |= std::regex_constants::icase;
        }
        if (flags & MULTILINE) {
            regex_flags |= std::regex_constants::multiline;
        }

        std::wregex re(widePattern, regex_flags);
        std::wstring wideResult = wideText;
        std::wsmatch match;

        size_t pos = 0;
        std::wstring buffer;

        while (std::regex_search(wideResult, match, re)) {
            // 添加匹配前的部分
            buffer += match.prefix().str();

            // 将匹配结果转换回string用于回调
            std::smatch narrowMatch;
            // 这里需要将wsmatch转换为smatch（简化处理）
            std::string matchStr = scd.wStrToStr(match.str());

            // 调用回调函数获取替换字符串
            std::string replacement = callback(narrowMatch);
            std::wstring wideReplacement = scd.strToWStr(replacement, true);

            // 添加替换后的内容
            buffer += wideReplacement;

            // 继续处理剩余部分
            wideResult = match.suffix().str();
        }

        // 添加最后剩余的部分
        buffer += wideResult;
        result = scd.wStrToStr(buffer);

    }
    catch (const std::regex_error& e) {
        std::cerr << "Regex error in sub(callback): " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in sub(callback): " << e.what() << std::endl;
    }

    return result;
}

#elif defined(WtlVs)
// 在 re 命名空间中修改 findAll 函数
vector<string> re::findAll(const string& pattern, const string& text, int flags)
{
    vector<string> results;
    wtl::StringCode scd;

    try {
        // 在VS中直接使用UTF-8字符串，避免宽字符转换问题
        std::string narrowText = text;
        std::string narrowPattern = pattern;

        // 处理flags
        std::regex_constants::syntax_option_type regex_flags = std::regex_constants::ECMAScript;
        if (flags & IGNORECASE)
        {
            regex_flags |= std::regex_constants::icase;
        }

        // VS中multiline模式需要特殊处理
        if (flags & MULTILINE)
        {
            // 对于多行模式，需要调整模式字符串
            if (narrowPattern.find("^") != std::string::npos ||
                narrowPattern.find("$") != std::string::npos)
            {
                // 如果模式包含^或$，可能需要特殊处理
                // 在VS中，默认情况下^和$匹配整个字符串的开始和结束
            }
        }

        // 使用标准正则表达式（UTF-8）
        std::regex re(narrowPattern, regex_flags);

        // 检查模式中是否有捕获组（括号）
        bool has_capture_groups = (pattern.find('(') != string::npos &&
            pattern.find(')') != string::npos);

        // 使用迭代器进行匹配
        auto words_begin = std::sregex_iterator(narrowText.begin(), narrowText.end(), re);
        auto words_end = std::sregex_iterator();

        // 遍历所有匹配结果
        for (std::sregex_iterator i = words_begin; i != words_end; ++i)
        {
            std::smatch match = *i;

            if (has_capture_groups && match.size() > 1)
            {
                // 有捕获组：返回所有捕获组的内容
                for (size_t j = 1; j < match.size(); ++j)
                {
                    if (match[j].matched)
                    {
                        results.push_back(match[j].str());
                    }
                }
            }
            else
            {
                // 没有捕获组：返回整个匹配
                results.push_back(match.str());
            }
        }

    }
    catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
        // 在VS中，某些正则表达式特性可能不被支持
        if (e.code() == std::regex_constants::error_complexity) {
            std::cerr << "Pattern too complex for Visual Studio regex engine" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return results;
}

// 同时修改 search 函数以支持捕获组 //
string re::search(const string& pattern, const string& text, int flags)
{
    wtl::StringCode scd;

    try {
        std::string narrowText = text;
        std::string narrowPattern = pattern;

        std::regex_constants::syntax_option_type regex_flags = std::regex_constants::ECMAScript;

        if (flags & IGNORECASE) {
            regex_flags |= std::regex_constants::icase;
        }

        std::regex re(narrowPattern, regex_flags);
        std::smatch match;

        if (std::regex_search(narrowText, match, re)) {
            // 如果有捕获组，返回第一个捕获组的内容
            // 如果没有捕获组，返回整个匹配
            if (match.size() > 1) {
                return match[1].str(); // 返回第一个捕获组
            }
            else {
                return match.str(); // 返回整个匹配
            }
        }

    }
    catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return "";
}

string re::sub(const string& pattern, const string& repl, const string& text, int flags)
{
    string result = text;

    try {
        std::string narrowText = text;
        std::string narrowPattern = pattern;
        std::string narrowRepl = repl;

        // 处理flags
        std::regex_constants::syntax_option_type regex_flags = std::regex_constants::ECMAScript;
        if (flags & IGNORECASE) {
            regex_flags |= std::regex_constants::icase;
        }

        // 创建正则表达式
        std::regex re(narrowPattern, regex_flags);

        // 执行替换操作
        result = std::regex_replace(narrowText, re, narrowRepl);

    }
    catch (const std::regex_error& e) {
        std::cerr << "Regex error in sub: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in sub: " << e.what() << std::endl;
    }

    return result;
}
#endif
