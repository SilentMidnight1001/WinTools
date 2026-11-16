#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <wtlDict.hpp>

using std::string;
using std::vector;

namespace wtl
{
    // 获取变量类型 //
    template<typename Types>
    string typeName(Types type)
    {
        auto temp_type = typeid(type).name();
        string type_name = temp_type;

        // 字符串部分 //
        string str_type = typeid(string).name();
        string c_str_type = typeid(const char *).name();
        string wc_str_type = typeid(wchar_t *).name();
        string w_str_type = typeid(std::wstring).name();

        //字符部分 //
        string char_type = typeid(char).name();
        string wchar_t_type = typeid(wchar_t).name();

        string unsigned_char = typeid(unsigned char).name();

        // 数字部分 //
        string int_type = typeid(int).name();
        string short_type = typeid(short).name();
        string long_type = typeid(long).name();
        string long_long_type = typeid(long long).name();

        string unsigned_int_type = typeid(unsigned int).name();
        string unsigned_short_type = typeid(unsigned short).name();
        string unsigned_long_type = typeid(unsigned long).name();
        string unsigned_long_long_type = typeid(unsigned long long).name();

        // 数组部分 //
        string int_arr_type = typeid(int *).name();
        string short_arr_type = typeid(short *).name();
        string long_arr_type = typeid(long *).name();
        string long_long_arr_type = typeid(long long *).name();

        string unsigned_int_arr_type = typeid(unsigned int *).name();
        string unsigned_short_arr_type = typeid(unsigned short *).name();
        string unsigned_long_arr_type = typeid(unsigned long *).name();
        string unsigned_long_long_arr_type = typeid(unsigned long long *).name();

        // 浮点数部分 //
        string double_type = typeid(double).name();
        string float_type = typeid(float).name();

        string double_arr_type = typeid(double *).name();
        string float_arr_type = typeid(float *).name();

        // 布尔类型部分 //
        string bool_type = typeid(bool).name();

        // vector字符与字符串 //
        string vector_char = typeid(vector<char>).name();
        string vector_string = typeid(vector<string>).name();

        string vector_unsigned_char = typeid(vector<unsigned char>).name();

        // 整数部分 //
        string vector_int = typeid(vector<int>).name();
        string vector_short = typeid(vector<short>).name();
        string vector_long = typeid(vector<long>).name();
        string vector_long_long = typeid(vector<long long>).name();

        string vector_unsigned_int = typeid(vector<unsigned int>).name();
        string vector_unsigned_short = typeid(vector<unsigned short >).name();
        string vector_unsigned_long = typeid(vector<unsigned long>).name();
        string vector_unsigned_long_long = typeid(vector<unsigned long long>).name();

        // 浮点数部分 //
        string vector_double = typeid(vector<double>).name();
        string vector_float = typeid(vector<float>).name();

        // 字符 //
        if (type_name == str_type)
        {
            return "string";
        }
        else if (type_name == char_type)
        {
            return "char";
        }
        else if (type_name == c_str_type)
        {
            return "c_str";
        }
        else if(type_name == unsigned_char)
        {
            return "unsigned_char";
        }
            // vector //
        else if (type_name == vector_int)
        {
            return "vector_int";
        }
        else if (type_name == vector_char)
        {
            return "vector_char";
        }
        else if (type_name == vector_string)
        {
            return "vector_string";
        }
        else if (type_name == vector_double)
        {
            return "vector_double";
        }
        else if (type_name == vector_float)
        {
            return "vector_float";
        }
        else if (type_name == vector_unsigned_char)
        {
            return "vector_unsigned_char";
        }
        else if (type_name == vector_unsigned_int)
        {
            return "vector_unsigned_int";
        }
        else if (type_name == vector_unsigned_long)
        {
            return "vector_unsigned_long";
        }
        else if (type_name == vector_unsigned_short)
        {
            return "vector_unsigned_short";
        }
        else if (type_name == vector_unsigned_long_long)
        {
            return "vector_unsigned_long_long";
        }
        else if (type_name == vector_short)
        {
            return "vector_short";
        }
        else if (type_name == vector_long)
        {
            return "vector_long";
        }
        else if (type_name == vector_long_long)
        {
            return "vector_long_long";
        }
            // 宽字符 //
        else if (type_name == wchar_t_type)
        {
            return "wchar_t";
        }
        else if(type_name == wc_str_type)
        {
            return "wc_str";
        }
        else if (type_name == w_str_type)
        {
            return "w_str";
        }
            // 数字 //
        else if (type_name == int_type)
        {
            return "int";
        }
        else if (type_name == double_type)
        {
            return "double";
        }
        else if (type_name == float_type)
        {
            return "float";
        }
        else if (type_name == long_type)
        {
            return "long";
        }
        else if (type_name == long_long_type)
        {
            return "long_long";
        }
        else if (type_name == short_type)
        {
            return "short";
        }
        else if (type_name == unsigned_int_type)
        {
            return "unsigned_int";
        }
        else if (type_name == unsigned_long_type)
        {
            return "unsigned_long";
        }
        else if (type_name == unsigned_short_type)
        {
            return "unsigned_short";
        }
        else if (type_name == unsigned_long_long_type)
        {
            return "unsigned_long_long";
        }
            // 布尔 //
        else if (type_name == bool_type)
        {
            return "bool";
        }
            // 数组 //
        else if (type_name == int_arr_type)
        {
            return "int_arr";
        }
        else if(type_name == long_arr_type)
        {
            return "long_arr";
        }
        else if (type_name == long_long_arr_type)
        {
            return "long_long_arr";
        }
        else if (type_name == short_arr_type)
        {
            return "short_arr";
        }
        else if (type_name == unsigned_int_arr_type)
        {
            return "unsigned_int_arr";
        }
        else if (type_name == unsigned_short_arr_type)
        {
            return "unsigned_short_arr_type";
        }
        else if (type_name == unsigned_long_arr_type)
        {
            return "unsigned_long_arr";
        }
        else if (type_name == unsigned_long_long_arr_type)
        {
            return "unsigned_long_long_arr";
        }
        else if (type_name == double_arr_type)
        {
            return "double_arr";
        }
        else if (type_name == float_arr_type)
        {
            return "float_arr";
        }

        // —————————— 新增：wtl::Dict 类型检测 ——————————
        // 先检查类型是否为类，再尝试匹配Dict特性
        if constexpr (std::is_class_v<Types>)
        {
            // 使用 is_aggregate 判断可能是结构体(pod)，否则可能是类
            // 但 Dict 是类，所以要走 else 分支
            // 这里我们使用更精确的检测：尝试匹配 Dict<*,*> 的模板实例
            // 由于无法直接获取模板参数，通过判断是否具有 keys/values/items 方法特征来确定
            // 更稳妥的方式：检查 typeid 名称是否包含 "Dict"
            if (type_name.find("Dict") != std::string::npos)
            {
                return "dict";
            }
        }

        // —————————— 新增：std::tuple 类型检测 ——————————
        // 检查类型名是否包含 "tuple"
        if (type_name.find("tuple") != std::string::npos ||
            type_name.find("__tuple") != std::string::npos) // MSVC 有时用不同修饰
        {
            return "tuple";
        }

        // —————————— 原有的 class/struct 检测（保持不变） ——————————
        // 检查类或者结构体 //
        if constexpr (std::is_class_v<Types>)
        {
            if constexpr (std::is_aggregate_v<Types>)
            {
                return "struct";
            } else
            {
                return "class";
            }
        }

        return "";
    }
}