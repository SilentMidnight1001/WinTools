#include "wtlPrintFormat.hpp"

// 压入颜色（用于嵌套） //
void wtl::Print::pushColor(const Color& color)
{
    colorStack.push_back(currentColor);
    setColor(color);
}

// 重置颜色 //
void wtl::Print::resetColor()
{
    currentColor = Color(); // 重置为无色
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 7); // 默认灰色
}

// 设置颜色（不影响现有接口） //
void wtl::Print::setColor(const Color& color)
{
    if (!color.isNoColor()) {
        currentColor = color;
        // 设置控制台颜色
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, color.getConsoleColor());
    }
}


void wtl::Print::popColor()
{
    if (!colorStack.empty()) {
        Color prevColor = colorStack.back();
        colorStack.pop_back();
        setColor(prevColor);
    }
    else {
        resetColor();
    }
}


/************** 映射部分给功能 **************/
// 检查字符串中花括号的数量
wtl::Print::BraceInfo wtl::Print::_countBraces(const std::string& str)
{
    size_t count = 0;
    bool valid_format = true;
    size_t auto_count = 0;      // 自动编号 {} / {:.2f} 的个数
    size_t max_index_end = 0;   // 显式索引 {N} 的最大值 + 1
    size_t len = str.length();
    size_t i = 0;

    // 整串必须都是数字才算合法索引（"2,3" 这类前缀数字不算） //
    auto is_all_digits = [](const std::string& s) {
        return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
    };

    while (i < len) {
        if (str[i] == '{') {
            // 检查是否是转义的 {{
            if (i + 1 < len && str[i + 1] == '{') {
                i += 2; // 跳过转义的 {{
                continue;
            }

            // 查找对应的 }
            size_t j = i + 1;
            while (j < len && str[j] != '}') {
                j++;
            }

            if (j < len && str[j] == '}') {
                // 注意：此处不能因 j 后面跟着 '}' 就当作转义跳过，
                // 否则 "{}}}"、"{{\"k\":{}}}" 中合法的 {} 占位符不会被计数，
                // 导致 println 判定为"无占位符"而退回空格拼接，与 _myFormat 不一致。
                // 独立出现的 }} 由下方 else if (str[i] == '}') 分支按转义处理。

                // 检查花括号内部内容
                std::string placeholder = str.substr(i + 1, j - i - 1);

                // 允许以下格式：
                // 1. 空花括号: {}
                // 2. 数字索引: {0}, {1}, {2}, ...
                // 3. 格式说明符: {:.2f}, {:.4f}, {:.6f}, ...
                // 4. 带索引的格式说明符: {0:.2f}, {1:.4f}, ...

                if (placeholder.empty()) {
                    // 自动编号: {}
                    auto_count++;
                }
                else {
                    // 拆成 索引部分 : 格式说明符，如 {0:.2f} -> "0" 与 ".2f"
                    size_t colon_pos = placeholder.find(':');
                    std::string index_part = (colon_pos == std::string::npos)
                                             ? placeholder
                                             : placeholder.substr(0, colon_pos);

                    if (colon_pos != std::string::npos) {
                        // 目前仅支持 .Nf 形式的格式说明符
                        std::string format_spec = placeholder.substr(colon_pos + 1);
                        if (format_spec.find('.') == std::string::npos ||
                            format_spec.find('f') == std::string::npos) {
                            valid_format = false;
                        }
                    }

                    if (index_part.empty()) {
                        // 自动编号带格式说明符: {:.2f}
                        auto_count++;
                    }
                    else if (is_all_digits(index_part)) {
                        // 显式索引: {0}、{1:.2f}
                        size_t idx = std::stoul(index_part);
                        // 不用 std::max：windows.h 的 max 宏会把它展开成非法的 std::(...)
                        if (idx + 1 > max_index_end) {
                            max_index_end = idx + 1;
                        }
                    }
                    else {
                        // 既不是索引也不是格式说明符，属普通文本中的花括号
                        // 如正则 {2,3}、集合 {a,b}、JSON {"k":1}
                        valid_format = false;
                    }
                }

                count++;
                i = j + 1; // 移动到 } 后面
            }
            else {
                valid_format = false; // 没有匹配的 }
                break;
            }
        }
        else if (str[i] == '}') {
            // 单独的 }，检查是否是转义的 }}
            if (i + 1 < len && str[i + 1] == '}') {
                i += 2; // 跳过转义的 }}
            }
            else {
                valid_format = false; // 无效的单独 }
                break;
            }
        }
        else
        {
            i++; // 普通字符，继续前进)
        }
    }

    // 格式化实际需要的参数个数：自动编号个数 与 最大显式索引+1 取较大者
    size_t required = (auto_count > max_index_end) ? auto_count : max_index_end;

    return { count, required, valid_format };
}

/********************** Color 实现 **********************/
void wtl::Color::_mapColorNameToRgb(const std::string& name)
{
    static std::map<std::string, std::tuple<int, int, int>> colorMap =
            {
                    {"black", {0, 0, 0}},
                    {"red", {255, 0, 0}},
                    {"green", {0, 255, 0}},
                    {"blue", {0, 0, 255}},
                    {"yellow", {255, 255, 0}},
                    {"magenta", {255, 0, 255}},
                    {"cyan", {0, 255, 255}},
                    {"white", {255, 255, 255}},
                    {"gray", {128, 128, 128}},
                    {"orange", {255, 165, 0}},
                    {"purple", {128, 0, 128}},
                    {"pink", {255, 192, 203}}
            };

    auto it = colorMap.find(name);
    if (it != colorMap.end()) {
        auto [r, g, b] = it->second;
        red = r; green = g; blue = b;
    }
    else {
        // 默认黑色
        red = 0; green = 0; blue = 0;
    }
}

// 解析十六进制颜色
bool wtl::Color::_parseHexColor(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return false;

    std::string hexValue = hex.substr(1);
    if (hexValue.length() == 3) {
        // #RGB 格式
        red = std::stoi(hexValue.substr(0, 1) + hexValue.substr(0, 1), nullptr, 16);
        green = std::stoi(hexValue.substr(1, 1) + hexValue.substr(1, 1), nullptr, 16);
        blue = std::stoi(hexValue.substr(2, 1) + hexValue.substr(2, 1), nullptr, 16);
    }
    else if (hexValue.length() == 6) {
        // #RRGGBB 格式
        red = std::stoi(hexValue.substr(0, 2), nullptr, 16);
        green = std::stoi(hexValue.substr(2, 2), nullptr, 16);
        blue = std::stoi(hexValue.substr(4, 2), nullptr, 16);
    }
    else {
        return false;
    }
    return true;
}
