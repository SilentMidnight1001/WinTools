#pragma once
/**
 * wtlFileManagement.hpp - 文件管理库（C++17 / Windows）
 *
 * 暴露 FileManagement 与 FileObject 类接口，实现位于对应 .cpp。
 * Exposes the FileManagement and FileObject interfaces; implementations are in the .cpp files.
 */

#include <filesystem>
#include <string>
#include <vector>

namespace wtl
{
    namespace fileManagementCode
    {
        // 匹配模式枚举
        // File name matching mode enumeration.
        enum class MatchMode
        {
            EXACT,      // 精确匹配 / Exact file name match.
            REGEX,      // 正则表达式 / Regular expression match.
            WHOLE_WORD, // 整词匹配 / Whole-word file name match.
            CONTAINS    // 包含匹配 / Substring match.
        };
    }

    /******************** 文件操作 ***************************/
    class FileManagement
    {
    public:
        // 删除文件函数
        // Deletes a file and clears the read-only attribute when necessary.
        bool removeFile(const std::string& fileName);
        // 创建文件夹函数
        // Creates a directory.
        bool createDir(const std::string& fileName);
        // 递归删除目录。
        // Recursively removes a directory.
        bool removeDir(const std::string& dirPath);
        // 写日志函数
        // Appends text to a log file.
        bool writeLog(const std::string& str, const std::string& fileName);
        // 复制目录到目标路径。
        // Copies a directory to the target path.
        bool copyDir(const std::filesystem::path& sourcePath, const std::filesystem::path& targetPath);
        // 获取目录树字符串。
        // Returns a directory tree string.
        std::string getFileTree(const std::string& rootPath);
    };

    /**
     * [第三方库] FileObject 文件目录对象
     *
     * 功能：
     * - 加载指定目录并递归保存子目录结构
     * - 按精确、正则、整词或包含模式搜索文件名
     * - 输出目录树、获取当前条目和全部文件路径
     * - 支持区分或不区分大小写的文件名匹配
     *
     * 说明：本类没有模板成员，因此所有方法均在 wtlFileManagement.cpp 中实现。
     * 若后续增加模板接口，模板实现必须保留在头文件中，供调用方实例化。
     *
     * Third-party directory object for loading, searching, and rendering a directory tree.
     * This class has no template members; all methods are implemented in wtlFileManagement.cpp.
     */
    class FileObject
    {
    private:
        // 文件名匹配模式 / File name matching mode.
        fileManagementCode::MatchMode model;
        // 是否区分大小写 / Whether matching is case-sensitive.
        bool caseSensitive;

        // 根据配置统一文件名和匹配串的字符大小写 //
        // Normalizes file names and patterns according to the case-sensitive setting.
        std::string _prepareString(const std::string& str) const;
        // 根据当前匹配模式判断文件名是否命中 //
        // Tests whether a file name matches the current mode.
        bool _matches(const std::string& fileName, const std::string& pattern) const;
        // 递归收集当前目录及子目录下的全部文件 //
        // Recursively collects all files below the current directory.
        void _collectAllFiles(std::vector<std::string>& result) const;
        // 递归搜索匹配文件名 //
        // Recursively searches for matching file names.
        void _searchRecursive(const std::string& pattern, std::vector<std::string>& results) const;

    public:
        // 根目录 / Root directory.
        std::string rootDir;
        // 当前目录下的文件名称 / File names in the current directory.
        std::vector<std::string> fileNames;
        // 当前目录下的文件夹名称（不包含递归内容）//
        // Directory names in the current directory, excluding recursive descendants.
        std::vector<std::string> dirNames;
        // 子目录对象，由 FileObject 负责释放 //
        // Child objects owned and released by FileObject.
        std::vector<FileObject*> subDirectories;

        // 创建空目录对象。
        // Creates an empty directory object.
        FileObject();

        // 加载指定目录，默认使用包含匹配且不区分大小写 //
        // Loads a directory using CONTAINS mode and case-insensitive matching by default.
        explicit FileObject(const std::string& path,
                            fileManagementCode::MatchMode mode = fileManagementCode::MatchMode::CONTAINS,
                            bool cs = false);

        // 递归释放子目录对象 //
        // Recursively releases child directory objects.
        ~FileObject();

        // 保持原第三方接口语义，禁止拷贝，允许移动 //
        // Preserves the original third-party semantics: copy is disabled and move is enabled.
        FileObject(const FileObject&) = delete;
        FileObject& operator=(const FileObject&) = delete;
        FileObject(FileObject&& other) noexcept;

        // 设置和读取文件名匹配模式 //
        // Sets or reads the file name matching mode.
        void setModel(fileManagementCode::MatchMode mode);
        fileManagementCode::MatchMode getModel() const;

        // 设置和读取是否区分大小写 //
        // Sets or reads the case-sensitive matching flag.
        void setCaseSensitive(bool cs);
        bool isCaseSensitive() const;

        // 递归搜索所有匹配的文件并返回完整路径 //
        // Recursively searches matching files and returns their full paths.
        std::vector<std::string> searchFiles(const std::string& pattern) const;

        // 将匹配模式转换为英文描述 //
        // Converts a matching mode to its English description.
        static std::string translateModelToEnglish(fileManagementCode::MatchMode mode);
        // 将英文描述转换回匹配模式，无法识别时返回 CONTAINS //
        // Converts an English description back to a matching mode; unknown values return CONTAINS.
        static fileManagementCode::MatchMode translateFromEnglish(const std::string& englishDesc);

        // 重新读取当前根目录，并重建子目录对象 //
        // Reloads the current root directory and rebuilds child directory objects.
        void loadDirectory();

        // 将目录树输出到标准输出 //
        // Prints the directory tree to standard output.
        void printTree(int depth = 0) const;
        // 获取目录树字符串 //
        // Returns the directory tree as a string.
        std::string getTreeString(int depth = 0) const;

        // 获取当前目录下的文件和文件夹条目，文件夹名以 "/" 结尾 //
        // Returns entries in the current directory; directory names end with "/".
        std::vector<std::string> getAllEntries() const;
        // 获取当前目录及所有子目录中的文件完整路径 //
        // Returns full file paths from the current directory and all descendants.
        std::vector<std::string> getAllFiles() const;
    };
}
