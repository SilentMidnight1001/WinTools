#include "wtlTime.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    std::tm _toTm(const wtl::time::struct_time& value)
    {
        std::tm result{};
        result.tm_year = value.tm_year - 1900;
        result.tm_mon = value.tm_mon - 1;
        result.tm_mday = value.tm_mday;
        result.tm_hour = value.tm_hour;
        result.tm_min = value.tm_min;
        result.tm_sec = value.tm_sec;
        result.tm_wday = value.tm_wday;
        result.tm_yday = value.tm_yday - 1;
        result.tm_isdst = value.tm_isdst;
        return result;
    }

    wtl::time::struct_time _fromTm(const std::tm& value)
    {
        wtl::time::struct_time result;
        result.tm_year = value.tm_year + 1900;
        result.tm_mon = value.tm_mon + 1;
        result.tm_mday = value.tm_mday;
        result.tm_hour = value.tm_hour;
        result.tm_min = value.tm_min;
        result.tm_sec = value.tm_sec;
        result.tm_wday = value.tm_wday;
        result.tm_yday = value.tm_yday + 1;
        result.tm_isdst = value.tm_isdst;
        return result;
    }

    bool _localTime(std::time_t raw, std::tm& result)
    {
        return localtime_s(&result, &raw) == 0;
    }

    bool _gmTime(std::time_t raw, std::tm& result)
    {
        return gmtime_s(&result, &raw) == 0;
    }
}

namespace wtl
{
    namespace time
    {
        // 返回当前 Unix 时间戳，单位为秒。
        // Returns the current Unix timestamp in seconds.
        double time()
        {
            const auto now = std::chrono::system_clock::now();
            return std::chrono::duration<double>(now.time_since_epoch()).count();
        }

        // 暂停当前线程指定秒数。
        // Suspends the current thread for the specified number of seconds.
        void sleep(double secs)
        {
            if (secs <= 0.0)
            {
                return;
            }

            std::this_thread::sleep_for(std::chrono::duration<double>(secs));
        }

        // 将当前时间转换为本地结构化时间。
        // Converts the current time to local structured time.
        struct_time localtime()
        {
            return localtime(time());
        }

        // 将指定时间戳转换为本地结构化时间。
        // Converts the specified timestamp to local structured time.
        struct_time localtime(double secs)
        {
            const std::time_t raw = static_cast<std::time_t>(secs);
            std::tm value{};
            if (!_localTime(raw, value))
            {
                throw std::runtime_error("wtl::time::localtime failed");
            }
            return _fromTm(value);
        }

        // 将当前时间转换为 UTC 结构化时间。
        // Converts the current time to UTC structured time.
        struct_time gmtime()
        {
            return gmtime(time());
        }

        // 将指定时间戳转换为 UTC 结构化时间。
        // Converts the specified timestamp to UTC structured time.
        struct_time gmtime(double secs)
        {
            const std::time_t raw = static_cast<std::time_t>(secs);
            std::tm value{};
            if (!_gmTime(raw, value))
            {
                throw std::runtime_error("wtl::time::gmtime failed");
            }
            return _fromTm(value);
        }

        // 将本地结构化时间转换为时间戳。
        // Converts local structured time to a timestamp.
        double mktime(const struct_time& value)
        {
            std::tm raw = _toTm(value);
            const std::time_t result = std::mktime(&raw);
            if (result == static_cast<std::time_t>(-1))
            {
                throw std::runtime_error("wtl::time::mktime failed");
            }
            return static_cast<double>(result);
        }

        // 按指定格式格式化结构化时间。
        // Formats structured time according to the specified format string.
        std::string strftime(const std::string& format, const struct_time& value)
        {
            if (format.empty())
            {
                return {};
            }

            std::tm raw = _toTm(value);
            std::vector<char> buffer(128);

            while (buffer.size() <= 1024 * 1024)
            {
                const std::size_t size = std::strftime(
                        buffer.data(),
                        buffer.size(),
                        format.c_str(),
                        &raw);

                if (size > 0)
                {
                    return std::string(buffer.data(), size);
                }

                buffer.resize(buffer.size() * 2);
            }

            throw std::runtime_error("wtl::time::strftime failed");
        }

        // 按指定格式解析时间字符串。
        // Parses a time string using the specified format.
        struct_time strptime(const std::string& text, const std::string& format)
        {
            std::tm raw{};
            raw.tm_year = 70;
            raw.tm_mon = 0;
            raw.tm_mday = 1;
            raw.tm_isdst = -1;

            std::istringstream input(text);
            input >> std::get_time(&raw, format.c_str());
            if (input.fail())
            {
                throw std::runtime_error("wtl::time::strptime failed");
            }

            input >> std::ws;
            if (!input.eof())
            {
                throw std::runtime_error("wtl::time::strptime contains trailing characters");
            }

            if (std::mktime(&raw) == static_cast<std::time_t>(-1))
            {
                throw std::runtime_error("wtl::time::strptime normalization failed");
            }

            return _fromTm(raw);
        }

        // 将当前本地时间转换为可读英文时间字符串。
        // Converts the current local time to a readable English time string.
        std::string asctime()
        {
            return asctime(localtime());
        }

        // 将结构化时间转换为可读英文时间字符串。
        // Converts structured time to a readable English time string.
        std::string asctime(const struct_time& value)
        {
            std::tm raw = _toTm(value);
            const char* result = std::asctime(&raw);
            if (result == nullptr)
            {
                throw std::runtime_error("wtl::time::asctime failed");
            }

            std::string text(result);
            if (!text.empty() && text.back() == '\n')
            {
                text.pop_back();
            }
            return text;
        }

        // 将当前时间戳转换为可读英文时间字符串。 //
        // Converts the current timestamp to a readable English time string.
        std::string ctime()
        {
            return ctime(time());
        }

        // 将指定时间戳转换为可读英文时间字符串。 //
        // Converts the specified timestamp to a readable English time string.
        std::string ctime(double secs)
        {
            return asctime(localtime(secs));
        }
    }
}
