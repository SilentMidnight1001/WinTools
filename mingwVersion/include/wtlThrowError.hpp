#pragma once
/**
 * ThrowError — Python 风格异常抛出库 (C++17 / Windows / header-only)
 *
 * 用法（零宏，直接调静态方法）:
 *   #include "ThrowError.hpp"
 *   ThrowError::showError("文件不存在");
 *
 * 输出格式（仿 Python traceback）:
 *   Traceback (most recent call last):
 *     File "D:\project\main.cpp", line 42, in main
 *       showError("文件不存在")
 *   Exception: 文件不存在
 *
 * 原理:
 *   __builtin_FILE() / __builtin_LINE() / __builtin_FUNCTION() 作为默认参数时，
 *   会在 调用点 求值，效果等价 C++20 的 std::source_location::current()。
 *   支持: MSVC 2019 16.6+ / GCC 4.8+ / Clang 3.5+
 */

#include <sstream>
#include <stdexcept>
#include <string>

namespace wtl
{
    class ThrowError
    {
    public:
        /**
         * 抛出 Python 风格 traceback 异常
         *
         * @param message  用户自定义报错信息
         * @param file     调用点文件路径（默认参数，编译器自动填入）
         * @param line     调用点行号
         * @param func     调用点函数名（MSVC 下含命名空间/类前缀）
         */
        [[noreturn]] static void showError(
                const std::string& message,
                const char* file = __builtin_FILE(),
                int line = __builtin_LINE(),
                const char* func = __builtin_FUNCTION())
        {
            // 从完整路径中提取文件名
            std::string fileName(file);
            auto pos = fileName.find_last_of("/\\");
            if (pos != std::string::npos) {
                fileName = fileName.substr(pos + 1);
            }

            std::ostringstream oss;
            oss << "Traceback (most recent call last):\n";
            oss << "  File \"" << file << "\" ," << fileName << ":" << line << ", in " << func << "\n";
            oss << "    showError(\"" << message << "\")\n";
            oss << "Exception: " << message;
            throw std::runtime_error(oss.str());
        }

        // 禁止实例化（纯工具类）
        ThrowError() = delete;
    };

}

