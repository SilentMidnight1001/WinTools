#pragma once
#include <algorithm>
#include <iomanip>
#include <windows.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wtlDict.hpp>
#include <wtlTypeName.hpp>
#include <wtlThrowError.hpp>

namespace wtl {
    /****************************** 控制台输出部分 ******************************/
    /********************** 控制台颜色输出 **********************/
    class Color
    {
    private:
        int red, green, blue;
        bool isSystemColor;
        std::string colorName;

        // 将颜色名称映射到RGB值
        void _mapColorNameToRgb(const std::string& name) ;


        // 解析十六进制颜色
        bool _parseHexColor(const std::string& hex);


    public:
        // 默认构造函数 - 无色
        Color() : red(-1), green(-1), blue(-1), isSystemColor(false) {}

        // RGB构造函数
        Color(int r, int g, int b) : red(r), green(g), blue(b), isSystemColor(false) {}

        // 颜色名称构造函数
        Color(const std::string& name) : isSystemColor(false)
        {
            if (name.empty()) {
                red = green = blue = -1;
            } else if (name[0] == '#') {
                if (!_parseHexColor(name))
                {
                    red = green = blue = -1;
                }
            } else {
                _mapColorNameToRgb(name);
            }
        }

        // 获取Windows控制台颜色属性
        WORD getConsoleColor() const
        {
            if (red == -1) return 7; // 默认灰色

            // 简单的RGB到控制台颜色的映射
            bool bright = (red > 192 || green > 192 || blue > 192);
            int color = 0;

            if (red > 128) color |= 4;   // 红色
            if (green > 128) color |= 2; // 绿色
            if (blue > 128) color |= 1;  // 蓝色

            return color | (bright ? 8 : 0);
        }

        // 检查是否是无色（默认）
        bool isNoColor() const
        {
            return red == -1 && green == -1 && blue == -1;
        }

        // 获取RGB值
        std::tuple<int, int, int> getRgb() const
        {
            return {red, green, blue};
        }
    };

    /*********************** Format与Print辅助功能类 **************************/
    /************ Format类 ************/
    class FormatWtl
    {
    public:
        // 辅助函数，支持std::string作为格式字符串
        template<typename... Args>
        std::string format(const std::string& fmt, Args&&... args);

        // 布尔处理函数 //
        /**
         * @brief 将布尔值转换为字符串表示
         *
         * @param boolType 需要转换的布尔值
         * @return std::string 返回布尔值对应的字符串，"true"或"false"
         */
        inline std::string _myToString(bool boolType)
        {
            return boolType ? "true" : "false";
        }


        // 基础类型的 _myToString 重载 //
        template<typename T>
        std::string _myToString(const T& value);

        // 处理 vector类型 //
        template<typename T>
        std::string _myToString(const std::vector<T>& vec);

        // 数组特化版本 //
        template<typename T, size_t N>
        std::string _myToString(const T (&arr)[N]);

        // 指针数组版本 //
        template<typename T>
        std::string _myToString(const T* arr, size_t size);

        // dict类型检查 - 修复版本 //
        template<typename K, typename V>
        std::string _myToString(wtl::Dict<K, V> dict);

        // map类型设置 //
        template<typename K, typename V>
        std::string _myToString(std::map<K, V> mp);

        // map类型设置 //
        template<typename K, typename V>
        std::string _myToString(std::unordered_map<K, V> mp);
    };


    /************ Print类 ************/
    class Print
    {
    private:
        inline static wtl::Color currentColor;
        inline static std::vector<wtl::Color> colorStack;

    public:
        // 布尔值判断 //
        inline std::string _myToString(bool boolType)
        {
            return boolType?"true":"false";
        }
        // 基础情况：处理单个参数到字符串的转换 //
        template<typename T>
        std::string _myToString(const T& arg);

        // map类型设置 //
        template<typename K, typename V>
        std::string _myToString(std::map<K, V> mp);

        // 无顺序map //
        template<typename K, typename V>
        std::string _myToString(std::unordered_map<K, V> mp);

        // dict类型检查 //
        template<typename K, typename V>
        std::string _myToString(wtl::Dict<K, V> dict);

        // 在 Print 类中添加数组特化版本 //
        template<typename T, size_t N>
        std::string _myToString(const T (&arr)[N]);

        // 或者使用模板特化 //
        template<typename T>
        std::string _myToString(const T* arr, size_t size);

        // 为 std::tuple 添加专门的 _myToString 函数
        template<typename... Args>
        std::string _myToString(const std::tuple<Args...>& tup);

        // 对 std::string 的特殊处理，避免不必要的转换 //
        inline std::string _myToString(const std::string& arg)
        {
            return arg;
        }

        // 对字符串字面量的特殊处理 //
        inline std::string _myToString(const char* const& arg)
        {
            return std::string(arg);
        }

        inline void gotoXy(int x, int y)
        {
            COORD coord;
            coord.X = x;
            coord.Y = y;
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        }

        inline void clearLine()
        {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

            DWORD written;
            // 清除当前行从光标位置到行末的内容
            FillConsoleOutputCharacter(
                    GetStdHandle(STD_OUTPUT_HANDLE),
                    ' ',
                    csbi.dwSize.X - csbi.dwCursorPosition.X,
                    csbi.dwCursorPosition,
                    &written
            );
            // 将光标移回行首
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), csbi.dwCursorPosition);
        }

        // 处理 vector 类型 //
        template<typename T>
        std::string _myToString(const std::vector<T>& vec);

        // 花括号扫描结果 //
        struct BraceInfo
        {
            size_t count    = 0;    // 花括号占位符总数
            size_t required = 0;    // 格式化实际需要的参数个数（自动编号个数 与 最大显式索引+1 取较大者）
            bool   valid    = true; // 占位符语法是否合法（{a,b}、{"k":1}、{2,3} 等为非法）
        };

        // 检查字符串中花括号的数量 //
        BraceInfo _countBraces(const std::string& str);

        // 核心函数：识别花括号并替换参数 //
        template<typename... Args>
        std::string _myFormat(const std::string& fmt_str, const Args&... args);

        // 简单的顺序输出所有参数 //
        template<typename... Args>
        std::string _simplePrint(const Args&... args);

        // 设置颜色（不影响现有接口） //
        static void setColor(const wtl::Color& color);

        // 重置颜色 //
        static void resetColor();

        // 压入颜色（用于嵌套） //
        static void pushColor(const wtl::Color& color);
        // 弹出颜色 //
        static void popColor();
    };

    // 内部声明 //
    static Print __pnt__;

    /*********************** 颜色文本包装器 **************************/
    class __ColoredText__
    {
    private:
        std::string text;
        wtl::Color color;

    public:
        __ColoredText__(const std::string& str, const wtl::Color& col) : text(str), color(col) {}
        __ColoredText__(const char* str, const wtl::Color& col) : text(str), color(col) {}

        template<typename T>
        __ColoredText__(const T& value, const wtl::Color& col) : text(__pnt__._myToString(value)), color(col) {}

        const std::string& getText() const { return text; }
        const wtl::Color& getColor() const { return color; }

        // 添加友元函数用于输出
        friend std::ostream& operator<<(std::ostream& os, const __ColoredText__& ct) {
            os << ct.text;
            return os;
        }
    };

    namespace color
    {
        // 辅助函数，便于创建彩色文本
        inline __ColoredText__ printColor(const std::string& text, const wtl::Color& color)
        {
            return __ColoredText__(text, color);
        }

        template<typename T>
        inline __ColoredText__ printColor(const T& value, const wtl::Color& color) {
            return __ColoredText__(value, color);
        }
    }

    /*********************** 参数顺序输出辅助 **************************/
    // 输出单个参数，彩色文本用自身颜色，输出完恢复外层颜色 //
    template<typename T>
    inline void __printOne__(const T& value, const wtl::Color& ambient)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, __ColoredText__>) {
            Print::setColor(value.getColor());
            std::cout << value.getText();
            ambient.isNoColor() ? Print::resetColor() : Print::setColor(ambient);
        }
        else {
            std::cout << __pnt__._myToString(value);
        }
    }

    // 顺序输出所有参数，参数之间自动插入空格（与 Python print 一致） //
    template<typename... Args>
    inline void __printJoin__(const wtl::Color& ambient, const Args&... args)
    {
        bool isFirst = true;
        auto output = [&](const auto& arg)
        {
            if (!isFirst) {
                std::cout << ' ';
            }
            isFirst = false;
            __printOne__(arg, ambient);
        };

        (output(args), ...); // C++17 折叠表达式展开参数包
    }

    /*********************** 格式化辅助 **************************/
    // 输出格式串中的字面量片段，并把转义的 }} 还原为单个 } //
    // （{{ 的还原在扫描循环里就地完成，此处只处理 }}）        //
    inline void __emitLiteral__(std::ostringstream& out, const std::string& text)
    {
        for (size_t k = 0; k < text.size(); ++k)
        {
            out << text[k];
            if (text[k] == '}' && k + 1 < text.size() && text[k + 1] == '}') {
                ++k; // 跳过成对的第二个 }
            }
        }
    }


    /***************************** println和print *****************************/
    // println //
    template<typename T, typename... Args>
    void println(const T &first, const Args &...rest);
    // 颜色输出println //
    template<typename T, typename... Args>
    void println(const wtl::Color &colors, const T &first, const Args &...rest);

    // 智能判断输出模式的 print 函数（不换行） //
    template<typename T, typename... Args>
    void print(const T& first, const Args&... rest);
    // 颜色输出print //
    template<typename T, typename... Args>
    void print(const wtl::Color &colors, const T &first, const Args &...rest);

    // 无输出 //
    inline void printUpdateLine()
    {
        ;
    }

    // 智能判断输出模式的 printUpdateLine 函数（不换行），控制台更新 //
    template<typename T, typename... Args>
    void printUpdateLine(const T& first, const Args&... rest);

    // 颜色输出print //
    template<typename T, typename... Args>
    void printUpdateLine(const wtl::Color &colors, const T &first, const Args &...rest);

    /************ format映射 ************/
    // 最终函数 //
    template<typename... Args>
    std::string format(const std::string& fmt, Args&&... args);

}  // namespace wtl

/************** Format **************/
// 格式化操作 //
template<typename... Args>
std::string wtl::FormatWtl::format(const std::string& fmt, Args&&... args)
{
    // 将参数包转换为字符串向量
    std::vector<std::string> argStrings = {wtl::__pnt__._myToString(std::forward<Args>(args))...};
    size_t totalArgs = sizeof...(args);

    std::ostringstream result;
    size_t pos = 0;
    size_t len = fmt.length();

    // 用于跟踪已使用的最大索引
    size_t maxUsedIndex = 0;

    while (pos < len)
    {
        // 查找下一个左花括号
        size_t openBrace = fmt.find('{', pos);

        if (openBrace == std::string::npos)
        {
            // 没有更多花括号，输出剩余部分
            wtl::__emitLiteral__(result, fmt.substr(pos));
            break;
        }

        // 输出花括号前的内容
        wtl::__emitLiteral__(result, fmt.substr(pos, openBrace - pos));

        // 检查是否是转义的花括号 {{
        if (openBrace + 1 < len && fmt[openBrace + 1] == '{')
        {
            result << '{';
            pos = openBrace + 2;
            continue;
        }

        // 查找对应的右花括号
        size_t closeBrace = fmt.find('}', openBrace + 1);
        if (closeBrace == std::string::npos)
        {
            // 没有找到配对的右花括号，原样输出
            result << fmt.substr(openBrace);
            break;
        }

        // 注意：此处不能因 closeBrace 后面跟着 '}' 就当作转义处理，
        // 否则 "{}}}"、"{{\"k\":{}}}" 中合法的 {} 占位符会被吞掉。
        // }} 的还原统一由 __emitLiteral__ 在输出字面量片段时完成。

        // 检查花括号内容是否为空（即 {}）- 顺序插入模式
        if (closeBrace == openBrace + 1)
        {
            // 空占位符，按顺序取参数
            if (maxUsedIndex >= totalArgs)
            {
                wtl::ThrowError::showError(
                    "Too few arguments for format string (fmt=\"" + fmt +
                    "\", args=" + std::to_string(totalArgs) + ")");
            }
            result << argStrings[maxUsedIndex];
            maxUsedIndex++;
            pos = closeBrace + 1;
        }
        else
        {
            // 花括号内有内容，如 {0}, {1} - 普通插入（索引）模式
            std::string placeholder = fmt.substr(openBrace + 1, closeBrace - openBrace - 1);

            // 尝试解析为数字索引（try 内只保留 stoul，避免越界异常被自身 catch 吞掉）
            size_t index = 0;
            bool isNumeric = true;
            try
            {
                index = std::stoul(placeholder);
            }
            catch (const std::exception&)
            {
                isNumeric = false;
            }

            if (isNumeric)
            {
                // 更新最大使用索引
                if (index >= maxUsedIndex)
                {
                    maxUsedIndex = index + 1;  // +1 因为索引是从0开始的
                }

                if (index >= totalArgs)
                {
                    wtl::ThrowError::showError(
                        "Argument index out of range (index=" + std::to_string(index) +
                        ", args=" + std::to_string(totalArgs) +
                        ", fmt=\"" + fmt + "\")");
                }
                result << argStrings[index];
            }
            else
            {
                // 不是数字，按顺序使用参数
                if (maxUsedIndex >= totalArgs)
                {
                    wtl::ThrowError::showError(
                        "Too few arguments for format string (fmt=\"" + fmt +
                        "\", args=" + std::to_string(totalArgs) + ")");
                }
                result << argStrings[maxUsedIndex];
                maxUsedIndex++;
            }
            pos = closeBrace + 1;
        }
    }

    // 检查是否所有参数都被使用
    if (maxUsedIndex < totalArgs)
    {
        wtl::ThrowError::showError(
            "Too many arguments for format string (fmt=\"" + fmt +
            "\", args=" + std::to_string(totalArgs) +
            ", used=" + std::to_string(maxUsedIndex) + ")");
    }

    return result.str();
}

// 基础类型的 _myToString 重载 - 这是关键！ //
template<typename T>
std::string wtl::FormatWtl::_myToString(const T& value)
{
    // 对于基础类型，使用 ostringstream 直接转换 //
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// 处理 vector 类型 //
template<typename T>
std::string wtl::FormatWtl::_myToString(const std::vector<T>& vec)
{
    if (vec.empty())
    {
        return "[]";
    }

    std::ostringstream oss;
    oss << "[";

    for (size_t i = 0; i < vec.size(); ++i)
    {
        // 直接使用通用的 _myToString 转换每个元素
        oss << _myToString(vec[i]);

        if (i < vec.size() - 1) {
            oss << ", ";
        }
    }
    oss << "]";
    return oss.str();
}

// 数组特化版本 //
template<typename T, size_t N>
std::string wtl::FormatWtl::_myToString(const T (&arr)[N])
{

    std::ostringstream oss;
    auto type = wtl::typeName(arr[0]);
    bool isStr = false;
    if (type == "c_str" || type == "string")
    {
        isStr = true;
    }
    oss << "[";
    for (size_t i = 0; i < N; ++i)
    {
        string str = isStr? format(R"("{}")", arr[i]): _myToString(arr[i]);
        oss << str;  // 递归调用
        if (i < N - 1) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

// 指针数组版本 //
template<typename T>
std::string wtl::FormatWtl::_myToString(const T* arr, size_t size)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < size; ++i)
    {
        oss << _myToString(arr[i]);  // 递归调用
        if (i < size - 1) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

// dict类型检查 - 修复版本 //
template<typename K, typename V>
std::string wtl::FormatWtl::_myToString(wtl::Dict<K, V> dict)
{
    if (dict.empty())
    {
        return "{}";
    }
    std::ostringstream oss;
    bool k_isStr = false;
    bool v_isStr = false;

    // 获取第一个元素来判断类型
    auto keys = dict.keys();
    auto values = dict.values();

    if (keys.empty() || values.empty()) {
        return "{}";
    }

    // 使用第一个元素来判断类型
    auto first_key_type = wtl::typeName(keys[0]);
    auto first_value_type = wtl::typeName(values[0]);

    k_isStr = (first_key_type == "string") ||
              (first_key_type == "c_str") ||
              (first_key_type == "char");

    v_isStr = (first_value_type == "string") ||
              (first_value_type == "c_str") ||
              (first_value_type == "char");

    oss << "{";
    int tempCount = 0;
    for (auto &[k, v] : dict.items())
    {
        // 处理键
        if (k_isStr) {
            oss << "\"" << wtl::__pnt__._myToString(k) << "\"";
        } else {
            oss << wtl::__pnt__._myToString(k);
        }

        oss << ": ";

        // 处理值
        if (v_isStr) {
            oss << "\"" << wtl::__pnt__._myToString(v) << "\"";
        } else {
            oss << wtl::__pnt__._myToString(v);
        }

        // 添加逗号分隔（最后一个元素不加）
        if (tempCount < dict.size() - 1) {
            oss << ", ";
        }
        tempCount++;
    }
    oss << "}";
    return oss.str();
}

// map类型设置 - 修复版本 //
template<typename K, typename V>
std::string wtl::FormatWtl::_myToString(std::map<K, V> mp)
{
    if (mp.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    bool k_isStr = false;
    bool v_isStr = false;

    // 使用 begin() 获取指向第一个元素的迭代器
    auto first_element = mp.begin();

    // 安全地检查键和值的类型
    std::string first_key_str = wtl::__pnt__._myToString(first_element->first);
    std::string first_value_str = wtl::__pnt__._myToString(first_element->second);

    // 检查键和值是否需要引号（基于第一个元素的类型推断）
    k_isStr = (wtl::typeName(first_element->first) == "string") ||
              (wtl::typeName(first_element->first) == "c_str") ||
              (wtl::typeName(first_element->first) == "char");

    v_isStr = (wtl::typeName(first_element->second) == "string") ||
              (wtl::typeName(first_element->second) == "c_str") ||
              (wtl::typeName(first_element->second) == "char");

    oss << "{";
    int tempCount = 0;
    for (auto it = mp.begin(); it != mp.end(); ++it)
    {
        // 处理键
        if (k_isStr) {
            oss << "\"" << wtl::__pnt__._myToString(it->first) << "\"";
        } else {
            oss << wtl::__pnt__._myToString(it->first);
        }

        oss << ": ";

        // 处理值
        if (v_isStr) {
            oss << "\"" << wtl::__pnt__._myToString(it->second) << "\"";
        } else {
            oss << wtl::__pnt__._myToString(it->second);
        }

        // 添加逗号分隔（最后一个元素不加）
        if (tempCount < mp.size() - 1) {
            oss << ", ";
        }
        tempCount++;
    }
    oss << "}";
    return oss.str();
}

// map无序类型设置 - 修复版本 //
template<typename K, typename V>
std::string wtl::FormatWtl::_myToString(std::unordered_map<K, V> mp)
{
    if (mp.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    bool k_isStr = false;
    bool v_isStr = false;

    // 使用 begin() 获取指向第一个元素的迭代器
    auto first_element = mp.begin();

    // 安全地检查键和值的类型
    std::string first_key_str = wtl::__pnt__._myToString(first_element->first);
    std::string first_value_str = wtl::__pnt__._myToString(first_element->second);

    // 检查键和值是否需要引号（基于第一个元素的类型推断）
    k_isStr = (wtl::typeName(first_element->first) == "string") ||
              (wtl::typeName(first_element->first) == "c_str") ||
              (wtl::typeName(first_element->first) == "char");

    v_isStr = (wtl::typeName(first_element->second) == "string") ||
              (wtl::typeName(first_element->second) == "c_str") ||
              (wtl::typeName(first_element->second) == "char");

    oss << "{";
    int tempCount = 0;
    for (auto it = mp.begin(); it != mp.end(); ++it)
    {
        // 处理键
        if (k_isStr) {
            oss << "\"" << wtl::__pnt__._myToString(it->first) << "\"";
        } else {
            oss << wtl::__pnt__._myToString(it->first);
        }

        oss << ": ";

        // 处理值
        if (v_isStr) {
            oss << "\"" << wtl::__pnt__._myToString(it->second) << "\"";
        } else {
            oss << wtl::__pnt__._myToString(it->second);
        }

        // 添加逗号分隔（最后一个元素不加）
        if (tempCount < mp.size() - 1) {
            oss << ", ";
        }
        tempCount++;
    }
    oss << "}";
    return oss.str();
}
/**************** print ****************/
template<typename T>
std::string wtl::Print::_myToString(const T& arg)
{
    std::ostringstream oss;
    oss << arg;
    return oss.str();
}

// dict类型检查 - 修复版本 //
// dict类型检查 - 修复版本，支持 JsonValue
template<typename K, typename V>
std::string wtl::Print::_myToString(wtl::Dict<K, V> dict)
{
    if (dict.empty())
    {
        return "{}";
    }

    std::ostringstream oss;
    oss << "{";
    int tempCount = 0;

    for (auto &[k, v] : dict.items())
    {
        // 处理键 - 总是加引号（JSON标准）
        oss << "\"" << __pnt__._myToString(k) << "\":";

        // 处理值 - 根据类型决定输出格式
        if constexpr (std::is_same_v<V, wtl::JsonValue>)
        {
            // 对于 JsonValue 类型，递归调用 jsonValueToString
            oss << jsonValueToString(v);
        }
        else
        {
            // 检查是否为嵌套字典
            std::string value_type = wtl::typeName(v);
            bool v_isStr = (value_type == "string") ||
                           (value_type == "c_str") ||
                           (value_type == "char");
            bool v_isDict = (value_type.find("Dict") != std::string::npos) ||
                            (value_type.find("dict") != std::string::npos);

            if (v_isStr) {
                oss << "\"" << __pnt__._myToString(v) << "\"";
            }
            else if (v_isDict) {
                oss << __pnt__._myToString(v);
            }
            else {
                oss << __pnt__._myToString(v);
            }
        }

        if (tempCount < dict.size() - 1) {
            oss << ", ";
        }
        tempCount++;
    }
    oss << "}";
    return oss.str();
}

// map类型设置 //
// map类型设置 - 修复版本
template<typename K, typename V>
std::string wtl::Print::_myToString(std::map<K, V> mp)
{
    if (mp.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    bool k_isStr = false;
    bool v_isStr = false;

    // 使用 begin() 获取指向第一个元素的迭代器
    auto first_element = mp.begin();

    // 安全地检查键和值的类型
    std::string first_key_str = __pnt__._myToString(first_element->first);
    std::string first_value_str = __pnt__._myToString(first_element->second);

    // 检查键和值是否需要引号（基于第一个元素的类型推断）
    k_isStr = (wtl::typeName(first_element->first) == "string") ||
              (wtl::typeName(first_element->first) == "c_str") ||
              (wtl::typeName(first_element->first) == "char");

    v_isStr = (wtl::typeName(first_element->second) == "string") ||
              (wtl::typeName(first_element->second) == "c_str") ||
              (wtl::typeName(first_element->second) == "char");

    oss << "{";
    int tempCount = 0;
    for (auto it = mp.begin(); it != mp.end(); ++it)
    {
        // 处理键
        if (k_isStr) {
            oss << "\"" << __pnt__._myToString(it->first) << "\"";
        } else {
            oss << __pnt__._myToString(it->first);
        }

        oss << ": ";

        // 处理值
        if (v_isStr) {
            oss << "\"" << __pnt__._myToString(it->second) << "\"";
        } else {
            oss << __pnt__._myToString(it->second);
        }

        // 添加逗号分隔（最后一个元素不加）
        if (tempCount < mp.size() - 1) {
            oss << ", ";
        }
        tempCount++;
    }
    oss << "}";
    return oss.str();
}

// 无顺序map //
template<typename K, typename V>
std::string wtl::Print::_myToString(std::unordered_map<K, V> mp)
{
    if (mp.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    bool k_isStr = false;
    bool v_isStr = false;

    // 使用 begin() 获取指向第一个元素的迭代器
    auto first_element = mp.begin();

    // 安全地检查键和值的类型
    std::string first_key_str = __pnt__._myToString(first_element->first);
    std::string first_value_str = __pnt__._myToString(first_element->second);

    // 检查键和值是否需要引号（基于第一个元素的类型推断）
    k_isStr = (wtl::typeName(first_element->first) == "string") ||
              (wtl::typeName(first_element->first) == "c_str") ||
              (wtl::typeName(first_element->first) == "char");

    v_isStr = (wtl::typeName(first_element->second) == "string") ||
              (wtl::typeName(first_element->second) == "c_str") ||
              (wtl::typeName(first_element->second) == "char");

    oss << "{";
    int tempCount = 0;
    for (auto it = mp.begin(); it != mp.end(); ++it)
    {
        // 处理键
        if (k_isStr) {
            oss << "\"" << __pnt__._myToString(it->first) << "\"";
        } else {
            oss << __pnt__._myToString(it->first);
        }

        oss << ": ";

        // 处理值
        if (v_isStr) {
            oss << "\"" << __pnt__._myToString(it->second) << "\"";
        } else {
            oss << __pnt__._myToString(it->second);
        }

        // 添加逗号分隔（最后一个元素不加）
        if (tempCount < mp.size() - 1) {
            oss << ", ";
        }
        tempCount++;
    }
    oss << "}";
    return oss.str();
}

// 在 Print 类中添加数组特化版本 //
template<typename T, size_t N>
std::string wtl::Print::_myToString(const T (&arr)[N])
{
    std::ostringstream oss;
    auto type = wtl::typeName(arr[0]);
    oss << "[";
    for (size_t i = 0; i < N; ++i)
    {
        if (type == "string" || type == "c_str")
        {
            auto temp = wtl::format(R"("{}")", arr[i]);
            oss << _myToString(temp);
        }
        else if (type == "char")
        {
            auto temp = wtl::format(R"('{}')", arr[i]);
            oss << _myToString(temp);
        }
        else
        {
            oss << _myToString(arr[i]);
        }
        if (i < N - 1) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

// 或者使用模板特化 //
template<typename T>
std::string wtl::Print::_myToString(const T* arr, size_t size)
{
    std::ostringstream oss;
    string type = wtl::typeName(arr[0]);
    oss << "[";
    for (size_t i = 0; i < size; ++i)
    {
        if (type == "string" || type == "c_str")
        {
            auto temp = wtl::format(R"("{}")", arr[i]);
            oss << _myToString(temp);
        }
        else if (type == "char")
        {
            auto temp = wtl::format(R"('{}')", arr[i]);
            oss << _myToString(temp);
        }
        else
        {
            oss << _myToString(arr[i]);
        }
        if (i < size - 1) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

// 元组输出 //
template<typename... Args>
std::string wtl::Print::_myToString(const std::tuple<Args...>& tup)
{
    if constexpr (sizeof...(Args) == 0) {
        return "()";
    }

    std::ostringstream oss;
    oss << "(";

    // 使用折叠表达式和递归展开元组
    std::apply([&oss, this](const auto&... args) {
        size_t index = 0;
        auto print_arg = [&oss, &index, this](const auto& arg) {
            // 处理字符串类型
            if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::string> ||
                          std::is_same_v<std::decay_t<decltype(arg)>, const char*>) {
                oss << "\"" << _myToString(arg) << "\"";
            } else {
                oss << _myToString(arg);
            }

            if (++index < sizeof...(args)) {
                oss << ", ";
            }
        };

        (print_arg(args), ...); // 折叠表达式展开元组
    }, tup);

    oss << ")";
    return oss.str();
}



template<typename T>
std::string wtl::Print::_myToString(const std::vector<T>& vec)
{
    bool vectorIsNull = vec.empty();
    if (!vectorIsNull)
    {
        std::ostringstream oss;
        auto type = wtl::typeName(vec[0]);
        oss << "[";
        for (size_t i = 0; i < vec.size(); ++i)
        {
            if (type == "string" || type == "c_str")
            {
                auto temp = wtl::format(R"("{}")", vec[i]);
                oss << _myToString(temp);
            }
            else if (type == "char")
            {
                auto temp = wtl::format(R"('{}')", vec[i]);
                oss << _myToString(temp);
            }
            else
            {
                oss << _myToString(vec[i]);
            }
            if (i < vec.size() - 1)
            {
                oss << ", ";
            }
        }
        oss << "]";
        return oss.str();
    }
    return "[]";
}

// 简单的顺序输出所有参数
template<typename... Args>
std::string wtl::Print::_simplePrint(const Args&... args)
{
    std::ostringstream result;
    size_t count = 0;
    size_t total = sizeof...(args);

    auto print_arg = [&](const auto& arg)
    {
        result << _myToString(arg);
        if (++count < total) {
            result << " ";
        }
    };

    (print_arg(args), ...); // C++17 折叠表达式
    return result.str();
}

// 核心函数：识别花括号并替换参数
template<typename... Args>
std::string wtl::Print::_myFormat(const std::string& fmt_str, const Args&... args)
{
    // 将参数包转换为字符串向量
    std::vector<std::string> arg_strings = {__pnt__._myToString(args)...};
    size_t total_args = sizeof...(args);

    std::ostringstream result;
    size_t pos = 0;
    size_t len = fmt_str.length();

    // 用于跟踪已使用的最大索引
    size_t max_used_index = 0;

    // 用于顺序模式的当前索引
    size_t current_index = 0;

    while (pos < len)
    {
        // 查找下一个左花括号
        size_t open_brace = fmt_str.find('{', pos);

        if (open_brace == std::string::npos)
        {
            // 没有找到更多花括号，输出剩余部分
            wtl::__emitLiteral__(result, fmt_str.substr(pos));
            break;
        }

        // 输出花括号之前的内容
        wtl::__emitLiteral__(result, fmt_str.substr(pos, open_brace - pos));

        // 检查是否是转义的 {{
        if (open_brace + 1 < len && fmt_str[open_brace + 1] == '{')
        {
            result << '{';
            pos = open_brace + 2;
            continue;
        }

        // 查找对应的闭合花括号
        size_t close_brace = fmt_str.find('}', open_brace + 1);
        if (close_brace == std::string::npos)
        {
            wtl::ThrowError::showError(
                "Unmatched '{' in format string (fmt=\"" + fmt_str + "\")");
        }

        // 注意：此处不能因 close_brace 后面跟着 '}' 就当作转义处理，
        // 否则 "{}}}"、"{{\"k\":{}}}" 中合法的 {} 占位符会被吞掉。
        // }} 的还原统一由 __emitLiteral__ 在输出字面量片段时完成。

        // 提取花括号内的内容
        std::string placeholder = fmt_str.substr(open_brace + 1, close_brace - open_brace - 1);

        // 查找冒号分隔符
        size_t colon_pos = placeholder.find(':');

        if (colon_pos != std::string::npos) {
            // 有格式说明符
            std::string index_part = placeholder.substr(0, colon_pos);
            std::string format_spec = placeholder.substr(colon_pos + 1);

            // 确定参数索引
            size_t arg_index = 0;

            if (index_part.empty()) {
                // 顺序模式: {:格式说明符}
                arg_index = current_index;
                current_index++;
            } else {
                // 索引模式: {索引:格式说明符}
                bool validIndex = true;
                try {
                    arg_index = std::stoul(index_part);
                } catch (const std::exception&) {
                    validIndex = false;
                }
                if (!validIndex) {
                    wtl::ThrowError::showError(
                        "Invalid index in format specifier (spec=\"" + placeholder +
                        "\", fmt=\"" + fmt_str + "\")");
                }
            }

            // 更新最大使用索引
            if (arg_index + 1 > max_used_index) {
                max_used_index = arg_index + 1;
            }

            if (arg_index >= total_args) {
                wtl::ThrowError::showError(
                    "Argument index out of range (index=" + std::to_string(arg_index) +
                    ", args=" + std::to_string(total_args) +
                    ", fmt=\"" + fmt_str + "\")");
            }

            // 应用格式说明符
            std::string formatted_value = arg_strings[arg_index];

            // 检查是否是浮点数精度指定
            if (format_spec.find('.') != std::string::npos && format_spec.find('f') != std::string::npos) {
                // 尝试提取精度
                try {
                    // 查找小数点位置
                    size_t dot_pos = format_spec.find('.');
                    if (dot_pos != std::string::npos) {
                        // 提取精度数字
                        std::string precision_str = format_spec.substr(dot_pos + 1);
                        precision_str.erase(std::remove_if(precision_str.begin(), precision_str.end(),
                                                           [](char c) { return !std::isdigit(c); }), precision_str.end());

                        if (!precision_str.empty()) {
                            int precision = std::stoi(precision_str);

                            // 检查参数是否是浮点数
                            // 这里需要检查原始参数类型，但我们现在只有字符串
                            // 可以尝试从原始参数获取浮点数值
                            // 为了简化，我们假设如果字符串可以被解析为浮点数，就应用精度

                            // 尝试解析为浮点数
                            char* endptr = nullptr;
                            double float_value = std::strtod(arg_strings[arg_index].c_str(), &endptr);

                            if (endptr != arg_strings[arg_index].c_str() && *endptr == '\0') {
                                // 成功解析为浮点数
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(precision) << float_value;
                                formatted_value = oss.str();
                            }
                        }
                    }
                } catch (const std::exception&) {
                    // 格式说明符解析失败，使用原始字符串
                }
            }

            result << formatted_value;
            pos = close_brace + 1;
        }
        else if (placeholder.empty())
        {
            // 这是一个空的 {} 占位符 - 顺序插入模式
            if (current_index >= total_args)
            {
                wtl::ThrowError::showError(
                    "Too few arguments for format string (fmt=\"" + fmt_str +
                    "\", args=" + std::to_string(total_args) + ")");
            }

            result << arg_strings[current_index];
            if (current_index + 1 > max_used_index) {
                max_used_index = current_index + 1;
            }
            current_index++;
            pos = close_brace + 1;
        }
        else
        {
            // 花括号内有内容，如 {0}, {1} - 普通插入（索引）模式
            // try 内只保留 stoul，避免越界异常被自身的 catch 吞掉
            size_t index = 0;
            bool isNumeric = true;
            try
            {
                index = std::stoul(placeholder);
            }
            catch (const std::exception&)
            {
                isNumeric = false;
            }

            if (isNumeric)
            {
                // 更新最大使用索引
                if (index + 1 > max_used_index)
                {
                    max_used_index = index + 1;
                }

                if (index >= total_args)
                {
                    wtl::ThrowError::showError(
                        "Argument index out of range (index=" + std::to_string(index) +
                        ", args=" + std::to_string(total_args) +
                        ", fmt=\"" + fmt_str + "\")");
                }

                result << arg_strings[index];
                pos = close_brace + 1;
            }
            else
            {
                // 不是数字索引，按顺序处理
                if (current_index >= total_args)
                {
                    wtl::ThrowError::showError(
                        "Too few arguments for format string (fmt=\"" + fmt_str +
                        "\", args=" + std::to_string(total_args) + ")");
                }

                result << arg_strings[current_index];
                if (current_index + 1 > max_used_index) {
                    max_used_index = current_index + 1;
                }
                current_index++;
                pos = close_brace + 1;
            }
        }
    }

    // 检查是否所有参数都被使用
    if (max_used_index < total_args)
    {
        wtl::ThrowError::showError(
            "Too many arguments for format string (fmt=\"" + fmt_str +
            "\", args=" + std::to_string(total_args) +
            ", used=" + std::to_string(max_used_index) + ")");
    }

    return result.str();
}




/************** 映射Println与Print **************/
// println实现 //
template<typename T, typename... Args>
void wtl::println(const T &first, const Args &...rest)
{
    // 检查第一个参数是否是格式字符串
    if constexpr (std::is_convertible_v<T, std::string> && sizeof...(rest) > 0) {
        std::string fmt_str = __pnt__._myToString(first);

        // 检查字符串中是否包含花括号
        auto braces = __pnt__._countBraces(fmt_str);

        if (braces.valid && braces.count > 0 && braces.required == sizeof...(rest)) {
            // 使用格式化输出
            std::string formatted = __pnt__._myFormat(fmt_str, rest...);
            std::cout << formatted << std::endl;
            return;
        }
    }

    // 普通输出
    print(first, rest...);
    std::cout << std::endl;
}

// 颜色输出 //
template<typename T, typename... Args>
void wtl::println(const wtl::Color &colors, const T &first, const Args &...rest)
{
    Print::setColor(colors);
    // 检查第一个参数是否是格式字符串
    if constexpr (std::is_convertible_v<T, std::string> && sizeof...(rest) > 0)
    {
        std::string fmt_str = __pnt__._myToString(first);

        // 检查字符串中是否包含花括号
        auto braces = __pnt__._countBraces(fmt_str);

        if (braces.valid && braces.count > 0 && braces.required == sizeof...(rest)) {
            // 使用格式化输出
            std::string formatted = __pnt__._myFormat(fmt_str, rest...);
            std::cout << formatted << std::endl;
            Print::resetColor();
            return;
        }
    }

    // 普通输出
    print(first, rest...);
    std::cout << std::endl;
    Print::resetColor();
}

// print实现 //
template<typename T, typename... Args>
void wtl::print(const T& first, const Args&... rest)
{
    // 检查是否需要格式化
    if constexpr (std::is_convertible_v<T, std::string> && sizeof...(rest) > 0) {
        std::string fmt_str = __pnt__._myToString(first);

        auto braces = __pnt__._countBraces(fmt_str);

        if (braces.valid && braces.count > 0 && braces.required == sizeof...(rest)) {
            std::string formatted = __pnt__._myFormat(fmt_str, rest...);
            std::cout << formatted;
            return;
        }
    }

    // 普通输出：参数之间自动空格分隔
    __printJoin__(wtl::Color(), first, rest...);
}

// 颜色输出 //
template<typename T, typename... Args>
void wtl::print(const wtl::Color &colors, const T &first, const Args &...rest)
{
    Print::setColor(colors);
    // 检查是否需要格式化
    if constexpr (std::is_convertible_v<T, std::string> && sizeof...(rest) > 0)
    {
        std::string fmt_str = __pnt__._myToString(first);

        auto braces = __pnt__._countBraces(fmt_str);

        if (braces.valid && braces.count > 0 && braces.required == sizeof...(rest)) {
            std::string formatted = __pnt__._myFormat(fmt_str, rest...);
            std::cout << formatted;
            Print::resetColor();
            return;
        }
    }

    // 普通输出：参数之间自动空格分隔
    __printJoin__(colors, first, rest...);

    Print::resetColor();
}

/************* 控制台刷新输出 *************/
// 颜色输出print //
template<typename T, typename... Args>
void wtl::printUpdateLine(const T &first, const Args &...rest)
{
    __pnt__.gotoXy(0,0);
    __pnt__.clearLine();
    wtl::print(first);
    printUpdateLine(rest...);
}

// 颜色输出print //
template<typename T, typename... Args>
void wtl::printUpdateLine(const wtl::Color &colors, const T &first, const Args &...rest)
{
    __pnt__.gotoXy(0,0);
    __pnt__.clearLine();
    wtl::print(colors,first);
    printUpdateLine(rest...);
}



/************** 映射Format **************/
// 最终函数 //
template<typename... Args>
std::string wtl::format(const std::string& fmt, Args&&... args)
{
    FormatWtl fmts;
    return fmts.format(fmt, std::forward<Args>(args)...);
}
