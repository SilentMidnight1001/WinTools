#pragma once
/**
 * wtlRegex.hpp — 正则表达式模块（C++17）
 *
 * 仅暴露 re 命名空间接口，实现位于 src/source/wtlRegex.cpp。
 *
 * 使用：
 *   auto numbers = re::findAll(R"(\d+)", "abc123def456");
 */

#include <functional>
#include <regex>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace re
{
    enum ReFlags {
        IGNORECASE = 1,
        I = 1,
        MULTILINE = 2,
        M = 2,
        NOMODEL = 0
    };

    // 转义正则元字符，返回可用于字面匹配的 pattern（类似 Python re.escape）
    string escape(const string &pattern);

    vector<string> findAll(const string &pattern, const string &text, int flags = NOMODEL);
    string search(const string &pattern, const string &text, int flags = NOMODEL);
    string sub(const string &pattern, const string &repl, const string &text, int flags = NOMODEL);

    // 支持回调函数的 sub 版本
    string sub(const string &pattern,
               std::function<string(const std::smatch&)> callback,
               const string &text,
               int flags = NOMODEL);
}
