#pragma once
#include <cctype>
#include <initializer_list>
#include <list>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wtlThrowError.hpp>

/****************** auxiliaryTools辅助命名空间 *******************/
namespace auxiliaryTools
{
    class __GetValueType__
    {
    public:
        // 检测是否为map的特征类
        template<typename T>
        struct is_std_map : std::false_type {};

        template<typename Key, typename T, typename Compare, typename Alloc>
        struct is_std_map<std::map<Key, T, Compare, Alloc>> : std::true_type {};
    };
}

namespace wtl
{
    // 前向声明 — 供 Dict::dumps() 使用
    class JsonValue;
    std::string jsonValueToString(const JsonValue& v);

    /**************** 字典 ****************/
    template<typename Key, typename Value>
    class Dict
    {
    private:
        using KeyValuePair = std::pair< Key, Value>;
        using ListType = std::list<KeyValuePair>;
        using MapType = std::unordered_map<Key, typename ListType::iterator>;

        ListType order_list;  // 维护插入顺序的链表
        MapType lookup_map;   // 提供快速查找的哈希表

        std::vector<Key> keyList;
        std::vector<Value> valueList;


    public:
        Dict() = default;

        // 重载 operator[] 以支持 dict[key] = value 语法 //
        Value& operator[](const Key& key)
        {
            auto it = lookup_map.find(key);
            if (it != lookup_map.end())
            {
                // 键已存在：返回对应值的引用（用于赋值）
                return it->second->second;
            }
            else
            {
                // 键不存在：插入新键值对（值默认初始化），并返回值的引用
                order_list.push_back({key, Value{}});
                auto list_iter = --order_list.end();
                lookup_map[key] = list_iter;
                return list_iter->second;
            }
        }

        // 初始化列表构造函数 - 支持 {{k1, v1}, {k2, v2}} 语法
        Dict(std::initializer_list<std::pair<Key, Value>> initList)
        {
            for (const auto& pair : initList)
            {
                insert(pair.first, pair.second);
            }
        }

        // 新增：const版本 operator[] (用于读取)
        const Value& operator[](const Key& key) const
        {
            auto it = lookup_map.find(key);
            if (it != lookup_map.end())
            {
                return it->second->second;
            }
            else
            {
                // 键不存在时抛出异常（或者返回默认值）
                wtl::ThrowError::showError("Key not found in dictionary");
            }
        }

        // 拷贝构造函数
        Dict(const Dict& other)
        {
            for (const auto& pair : other.order_list)
            {
                insert(pair.first, pair.second);
            }
        }

        // 可选：添加安全的 get 方法，避免异常
        Value get(const Key& key, const Value& defaultValue = Value{}) const
        {
            auto it = lookup_map.find(key);
            if (it != lookup_map.end())
            {
                return it->second->second;
            }
            return defaultValue;
        }

        // 静态方法：从 JSON 字符串解析为 Dict (loads)
        static Dict<Key, Value> loads(const std::string& jsonStr);

        // Dict 转 JSON 字符串 (dumps) — 实现在 JsonValue 定义之后
        std::string dumps() const;
        // 插入或更新键值对 //
        inline void insert(const Key& key, const Value& value)
        {
            (*this)[key] = value; // 复用 operator[]
        }

        // 删除键值对 //
        inline bool pop(const Key& key)
        {
            auto it = lookup_map.find(key);
            if (it != lookup_map.end())
            {
                order_list.erase(it->second); // 从链表删除
                lookup_map.erase(it);         // 从哈希表删除
                return true;
            }
            return false;
        }

        // 判断 key 是否存在 //
        inline bool contains(const Key& key) const
        {
            return lookup_map.find(key) != lookup_map.end();
        }


        // 返回用于范围循环的 items 视图（支持结构化绑定） //
        inline auto items() const
        {
            return order_list; // 直接返回链表，其迭代器解引用为 std::pair<const Key, Value>
        }

        // update 合并另一个 dict //
        inline void update(const Dict<Key, Value>& other)
        {
            for (const auto& [k, v] : other.items()) {
                (*this)[k] = v;
            }
        }

        // setDefault //
        inline Value& setDefault(const Key& key, const Value& defaultValue)
        {
            auto it = lookup_map.find(key);
            if (it != lookup_map.end()) {
                return it->second->second;
            }
            insert(key, defaultValue);
            return lookup_map[key]->second->second;
        }

        inline auto keys() const
        {
            std::vector<Key> keyList;
            for (const auto& pair : order_list) {
                keyList.push_back(pair.first);
            }
            return keyList;
        }

        // 修复 values() 方法
        auto values() const
        {
            std::vector<Value> valueList;
            for (const auto& pair : order_list) {
                valueList.push_back(pair.second);
            }
            return valueList;
        }

        // 字典转为字符串 //
        std::string dictToString();

        // 其他常用方法
        inline size_t size() const { return order_list.size(); }
        inline bool empty() const { return order_list.empty(); }

        inline void clear()
        {
            order_list.clear();
            lookup_map.clear();
        }
    };

    class JsonValue
    {
    public:
        enum Type { Null, String, Number, Bool, Array, Object };

        JsonValue() : type(Null) {}
        JsonValue(const std::string& v) : type(String), strVal(v) {}
        JsonValue(double v) : type(Number), numVal(v) {}
        JsonValue(bool v) : type(Bool), boolVal(v) {}
        JsonValue(const std::vector<JsonValue>& v) : type(Array), arrVal(v) {}
        JsonValue(const Dict<std::string, JsonValue>& v) : type(Object), objVal(v) {}

        Type type = Null;

        std::string strVal;
        double numVal = 0;
        bool boolVal = false;
        std::vector<JsonValue> arrVal;
        Dict<std::string, JsonValue> objVal;
    };

    std::string jsonValueToString(const wtl::JsonValue& v);

// Dict::dumps() 实现 — 必须在 JsonValue 和 jsonValueToString 声明之后
    template<typename Key, typename Value>
    inline std::string Dict<Key, Value>::dumps() const
    {
        return jsonValueToString(JsonValue(*this));
    }

    Dict<std::string, JsonValue> stringToJson(const std::string& jsonStr);

    // ==================== 辅助工具 ====================
    namespace detail {
        // 基础类型 → 字符串（避免依赖 Print/__pnt__）
        template<typename T>
        inline std::string _toStr(const T& v) {
            if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                                std::is_same_v<std::decay_t<T>, char*>) {
                return std::string(v);
            } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
                return std::string(1, v);
            } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
                return v ? "true" : "false";
            } else {
                std::ostringstream oss;
                oss << v;
                return oss.str();
            }
        }

    }

    // ==================== Dict::dictToString ====================
    template<typename K, typename V>
    std::string Dict<K, V>::dictToString()
    {
        std::ostringstream oss;
        oss << "{";

        bool first = true;
        for (auto &[k, v] : items())
        {
            if (!first) oss << ",";
            first = false;

            oss << "\"" << detail::_toStr(k) << "\":";

            if constexpr (std::is_same_v<V, Dict<K, V>>)
            {
                oss << v.dictToString();
            }
            else if constexpr (std::is_same_v<V, JsonValue>)
            {
                oss << jsonValueToString(v);
            }
            else
            {
                oss << detail::_toStr(v);
            }
        }
        oss << "}";
        return oss.str();
    }

}  // namespace wtl

// ==================== StringToDictClass (全局命名空间) ====================
class StringToDictClass
{
private:
    size_t pos = 0;
    std::string json;

public:
    // 工具函数：去除字符串两端的空白字符
    std::string _trim(const std::string& str);

    // 跳过空白字符
    inline void _skipWhitespace()
    {
        while (pos < json.length() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            pos++;
        }
    }

    // 查看当前字符（不移动位置）
    inline char _peek()
    {
        if (pos >= json.length()) return '\0';
        return json[pos];
    }

    // 解析字符串值
    std::string _parseString();

    // 解析数字
    double _parseNumber();

    // 解析布尔值
    bool _parseBool();

    // 解析 null
    inline void _parseNull()
    {
        _skipWhitespace();
        if (json.compare(pos, 4, "null") == 0)
        {
            pos += 4;
        }
    }

    // 解析数组
    std::vector<wtl::JsonValue> _parseArray();

    // 解析对象
    wtl::Dict<std::string, wtl::JsonValue> _parseObject();

    // 主解析函数 - 完整实现
    wtl::JsonValue _parseJsonValue();

    // 公共解析接口
    inline wtl::JsonValue _parseJson(const std::string& jsonStr)
    {
        pos = 0;
        json = jsonStr;
        return _parseJsonValue();
    }

    template<typename Value>
    static Value _convertJsonValueToType(const wtl::JsonValue& v);
};

// ==================== _convertJsonValueToType ====================
template<typename Value>
Value StringToDictClass::_convertJsonValueToType(const wtl::JsonValue& v)
{
    if constexpr (std::is_same_v<Value, std::string>) {
        if (v.type == wtl::JsonValue::String) return v.strVal;
        if (v.type == wtl::JsonValue::Number) return std::to_string(v.numVal);
        if (v.type == wtl::JsonValue::Bool) return v.boolVal ? "true" : "false";
        return "";
    }
    else if constexpr (std::is_same_v<Value, double>) {
        if (v.type == wtl::JsonValue::Number) return v.numVal;
        if (v.type == wtl::JsonValue::String) return std::stod(v.strVal);
        return 0.0;
    }
    else if constexpr (std::is_same_v<Value, int>) {
        if (v.type == wtl::JsonValue::Number) return static_cast<int>(v.numVal);
        if (v.type == wtl::JsonValue::String) return std::stoi(v.strVal);
        return 0;
    }
    else if constexpr (std::is_same_v<Value, bool>) {
        if (v.type == wtl::JsonValue::Bool) return v.boolVal;
        if (v.type == wtl::JsonValue::Number) return v.numVal != 0;
        if (v.type == wtl::JsonValue::String) return v.strVal == "true";
        return false;
    }
    else if constexpr (std::is_same_v<Value, wtl::JsonValue>) {
        return v;  // 直接返回
    }
    else if constexpr (auxiliaryTools::__GetValueType__::is_std_map<Value>::value) {
        // 处理 map 类型
        Value result;
        if (v.type == wtl::JsonValue::Object) {
            for (const auto& [k, val] : v.objVal.items()) {
                using MappedType = typename Value::mapped_type;
                result.insert({k, _convertJsonValueToType<MappedType>(val)});
            }
        }
        return result;
    }
    else if constexpr (std::is_same_v<Value, wtl::Dict<std::string, wtl::JsonValue>>) {
        // 直接返回对象的深拷贝
        if (v.type == wtl::JsonValue::Object) {
            return v.objVal;
        }
        return wtl::Dict<std::string, wtl::JsonValue>();
    }
    else {
        // 默认返回空值
        return Value{};
    }
}

// ==================== Dict::loads 实现 ====================
template<typename Key, typename Value>
wtl::Dict<Key, Value> wtl::Dict<Key, Value>::loads(const std::string& jsonStr)
{
    StringToDictClass parser;
    wtl::JsonValue jsonValue = parser._parseJson(jsonStr);

    if (jsonValue.type != wtl::JsonValue::Object) {
        wtl::ThrowError::showError("JSON root must be an object for Dict::loads");
    }

    wtl::Dict<Key, Value> result;

    // 使用已有的静态方法 _convertJsonValueToType
    for (const auto& [k, v] : jsonValue.objVal.items()) {
        result.insert(k, StringToDictClass::_convertJsonValueToType<Value>(v));
    }
    return result;
}
