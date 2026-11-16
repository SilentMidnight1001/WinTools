#pragma once
/**
 * wtlTime.hpp - Python time 风格时间工具库（C++17 / Windows）
 *
 * 提供时间戳、延迟、本地时间、UTC 时间、时间结构体转换与格式化功能。
 * 非模板实现位于 wtlTime.cpp，可单独构建为 wtlTime。
 */

#include <string>

namespace wtl
{
    namespace time
    {
        /**
         * 与 Python time.struct_time 对应的结构化时间。
         *
         * tm_year: 完整年份，例如 2026
         * tm_mon:  月份，范围 1-12
         * tm_mday: 日期，范围 1-31
         * tm_hour: 小时，范围 0-23
         * tm_min:  分钟，范围 0-59
         * tm_sec:  秒，范围 0-60
         * tm_wday: 星期，范围 0-6，Monday=0
         * tm_yday: 一年中的第几天，范围 1-366
         * tm_isdst: 夏令时标志，正数有效，0 无效，负数未知
         */
        struct struct_time
        {
            int tm_year = 1970;
            int tm_mon = 1;
            int tm_mday = 1;
            int tm_hour = 0;
            int tm_min = 0;
            int tm_sec = 0;
            int tm_wday = 0;
            int tm_yday = 1;
            int tm_isdst = -1;
        };

        // 返回当前 Unix 时间戳，单位为秒，保留小数部分。
        // Returns the current Unix timestamp in seconds, including fractional seconds.
        double time();

        // 暂停当前线程指定秒数。
        // Suspends the current thread for the specified number of seconds.
        void sleep(double secs);

        // 将时间戳转换为本地结构化时间，不传参数时使用当前时间。
        // Converts a timestamp to local structured time; uses the current time when omitted.
        struct_time localtime();
        struct_time localtime(double secs);

        // 将时间戳转换为 UTC 结构化时间，不传参数时使用当前时间。
        // Converts a timestamp to UTC structured time; uses the current time when omitted.
        struct_time gmtime();
        struct_time gmtime(double secs);

        // 将本地结构化时间转换为时间戳。
        // Converts local structured time to a timestamp.
        double mktime(const struct_time& value);

        // 按指定格式格式化结构化时间。
        // Formats structured time according to the specified format string.
        std::string strftime(const std::string& format, const struct_time& value);

        // 按指定格式解析时间字符串，返回结构化时间。
        // Parses a time string with the specified format and returns structured time.
        struct_time strptime(const std::string& text, const std::string& format);

        // 将结构化时间转换为可读英文时间字符串。
        // Converts structured time to a readable English time string.
        std::string asctime();
        std::string asctime(const struct_time& value);

        // 将时间戳转换为可读英文时间字符串。
        // Converts a timestamp to a readable English time string.
        std::string ctime();
        std::string ctime(double secs);
    }
}
