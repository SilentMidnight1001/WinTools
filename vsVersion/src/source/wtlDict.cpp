#include "wtlDict.hpp"

#include <iostream>


/********************* 字典的CPP处理 ************************/
std::string wtl::jsonValueToString(const wtl::JsonValue& v)
{
    switch (v.type)
    {
        case JsonValue::String:
            return "\"" + v.strVal + "\"";
        case JsonValue::Number:
            return std::to_string(v.numVal);
        case JsonValue::Bool:
            return v.boolVal ? "true" : "false";
        case JsonValue::Array: {
            std::string s = "[";
            for (size_t i = 0; i < v.arrVal.size(); ++i) {
                if (i) s += ",";
                s += jsonValueToString(v.arrVal[i]);
            }
            return s + "]";
        }
        case JsonValue::Object: {
            std::string s = "{";
            bool first = true;
            for (auto& [k, val] : v.objVal.items()) {
                if (!first) s += ",";
                first = false;
                s += "\"" + k + "\":" + jsonValueToString(val);
            }
            return s + "}";
        }
        default:
            return "null";
    }
}


/*********************** StringToDictClass ************************/
// 工具函数：去除字符串两端的空白字符
std::string StringToDictClass::_trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

// 解析字符串值
std::string StringToDictClass::_parseString()
{
    _skipWhitespace();
    if (pos >= json.length() || json[pos] != '"') {
        return "";
    }

    pos++; // 跳过开始的 "
    std::string result;

    while (pos < json.length()) {
        char c = json[pos];
        if (c == '"') {
            pos++; // 跳过结束的 "
            break;
        } else if (c == '\\') {
            // 处理转义字符
            pos++;
            if (pos < json.length()) {
                char escaped = json[pos];
                switch (escaped) {
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    default: result += escaped; break;
                }
                pos++;
            }
        } else {
            result += c;
            pos++;
        }
    }

    return result;
}

// 解析数字 - 增加错误处理
double StringToDictClass::_parseNumber()
{
    _skipWhitespace();
    size_t start = pos;

    // 处理负号
    if (pos < json.length() && json[pos] == '-') {
        pos++;
    }

    // 整数部分
    while (pos < json.length() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        pos++;
    }

    // 小数部分
    if (pos < json.length() && json[pos] == '.') {
        pos++;
        while (pos < json.length() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
            pos++;
        }
    }

    // 指数部分
    if (pos < json.length() && (json[pos] == 'e' || json[pos] == 'E')) {
        pos++;
        if (pos < json.length() && (json[pos] == '+' || json[pos] == '-')) {
            pos++;
        }
        while (pos < json.length() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
            pos++;
        }
    }

    std::string numStr = json.substr(start, pos - start);

    // 增加空字符串检查
    if (numStr.empty()) {
        return 0.0;
    }

    try {
        return std::stod(numStr);
    } catch (const std::invalid_argument& e) {
        // 无效的数字格式
        std::cerr << "Warning: Invalid number format: " << numStr << std::endl;
        return 0.0;
    } catch (const std::out_of_range& e) {
        // 数字超出范围
        std::cerr << "Warning: Number out of range: " << numStr << std::endl;
        return 0.0;
    }
}

// 解析布尔值
bool StringToDictClass::_parseBool()
{
    _skipWhitespace();
    if (json.compare(pos, 4, "true") == 0) {
        pos += 4;
        return true;
    } else if (json.compare(pos, 5, "false") == 0) {
        pos += 5;
        return false;
    }
    return false;
}

// 解析数组
std::vector<wtl::JsonValue> StringToDictClass::_parseArray()
{
    std::vector<wtl::JsonValue> arr;

    _skipWhitespace();
    if (pos >= json.length() || json[pos] != '[') {
        return arr;
    }
    pos++; // 跳过 [

    _skipWhitespace();

    // 空数组
    if (pos < json.length() && json[pos] == ']') {
        pos++;
        return arr;
    }

    while (pos < json.length()) {
        wtl::JsonValue value = _parseJsonValue();
        arr.push_back(value);

        _skipWhitespace();
        if (pos >= json.length()) break;

        if (json[pos] == ',') {
            pos++;
            _skipWhitespace();
            continue;
        } else if (json[pos] == ']') {
            pos++;
            break;
        } else {
            break;
        }
    }

    return arr;
}

// 解析对象
wtl::Dict<std::string, wtl::JsonValue> StringToDictClass::_parseObject()
{
    wtl::Dict<std::string, wtl::JsonValue> obj;

    _skipWhitespace();
    if (pos >= json.length() || json[pos] != '{') {
        return obj;
    }
    pos++; // 跳过 {

    _skipWhitespace();

    // 空对象
    if (pos < json.length() && json[pos] == '}') {
        pos++;
        return obj;
    }

    while (pos < json.length()) {
        _skipWhitespace();

        // 解析键
        std::string key = _parseString();

        _skipWhitespace();
        if (pos >= json.length() || json[pos] != ':') {
            break;
        }
        pos++; // 跳过 :

        // 解析值
        wtl::JsonValue value = _parseJsonValue();
        obj.insert(key, value);

        _skipWhitespace();
        if (pos >= json.length()) break;

        if (json[pos] == ',') {
            pos++;
            _skipWhitespace();
            continue;
        } else if (json[pos] == '}') {
            pos++;
            break;
        } else {
            break;
        }
    }

    return obj;
}

// 主解析函数 - 完整实现
wtl::JsonValue StringToDictClass::_parseJsonValue()
{
    _skipWhitespace();

    if (pos >= json.length()) {
        return wtl::JsonValue();
    }

    char c = json[pos];

    if (c == '"') {
        // 字符串
        std::string str = _parseString();
        wtl::JsonValue value;
        value.type = wtl::JsonValue::String;
        value.strVal = str;
        return value;
    }
    else if (c == '{') {
        // 对象
        wtl::JsonValue value;
        value.type = wtl::JsonValue::Object;
        value.objVal = _parseObject();
        return value;
    }
    else if (c == '[') {
        // 数组
        wtl::JsonValue value;
        value.type = wtl::JsonValue::Array;
        value.arrVal = _parseArray();
        return value;
    }
    else if (c == 't' || c == 'f') {
        // 布尔值
        bool b = _parseBool();
        wtl::JsonValue value;
        value.type = wtl::JsonValue::Bool;
        value.boolVal = b;
        return value;
    }
    else if (c == 'n') {
        // null - 检查 "null" 关键字
        if (json.compare(pos, 4, "null") == 0) {
            pos += 4;
            return wtl::JsonValue();
        }
        // 如果不是 null，可能是其他以 n 开头的无效值
        return wtl::JsonValue();
    }
    else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        // 数字
        double num = _parseNumber();
        wtl::JsonValue value;
        value.type = wtl::JsonValue::Number;
        value.numVal = num;
        return value;
    }

    return wtl::JsonValue();
}
