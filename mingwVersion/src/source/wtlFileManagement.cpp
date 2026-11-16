#include "wtlFileManagement.hpp"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <functional>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <wtlStringCode.hpp>

namespace wtl
{
    namespace
    {
        StringCode scd;
    }

    // 删除文件 //
    bool FileManagement::removeFile(const std::string& fileName)
    {
        std::wstring wfileName = std::filesystem::u8path(fileName).wstring();
        const wchar_t* filePath = wfileName.c_str();

        // 去掉只读属性（可选）
        DWORD attrs = GetFileAttributesW(filePath);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY))
        {
            SetFileAttributesW(filePath, attrs & ~FILE_ATTRIBUTE_READONLY);
        }

        // 直接删除文件
        if (DeleteFileW(filePath))
        {
            return true;
        }
        else
        {
            std::cerr << scd.utf8ToGbk("删除失败，错误代码: ") << GetLastError() << std::endl;
            return false;
        }
    }

    // 创建文件夹 //
    bool FileManagement::createDir(const std::string& fileName)
    {
        // 要创建的文件夹路径（UTF-8 -> 宽字符）
        std::wstring dirName = std::filesystem::u8path(fileName).wstring();

        LPCWSTR folderPath = dirName.c_str();

        // 调用 Windows API 创建文件夹
        BOOL result = CreateDirectoryW(
                folderPath,        // 文件夹路径
                NULL                // 安全属性（可选参数，设为NULL使用默认）
        );

        // 检查是否创建成功
        if (result)
        {
            return true;
        }
        else
        {
            // 获取错误代码并提示
            return false;
        }
    }

    // 删除文件夹 //
    bool FileManagement::removeDir(const std::string& dirPath)
    {
        try
        {
            // 检查路径是否存在且是目录
            if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath))
            {
                // 递归删除目录及其内容
                std::filesystem::remove_all(dirPath);
                return true;
            }
            else
            {
                std::cerr << "Path does not exist or is not a directory: " << scd.utf8ToGbk(dirPath) << "\n";
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "Filesystem error\n";
            std::cerr << "Path: " << e.path1() << "\n";
            return false;
        }
        catch (const std::exception&)
        {
            std::cerr << "General error\n";
            return false;
        }
    }

    // 写入日志 //
    bool FileManagement::writeLog(const std::string& str, const std::string& fileName)
    {
        // 1. 使用正确的文件名参数打开文件（UTF-8 -> 宽字符） //
        FILE* file = _wfopen(std::filesystem::u8path(fileName).wstring().c_str(), L"a");

        if (file == nullptr)
        {
            printf("Unable to open the log file!\n");
            return false;
        }

        // 2. 正确写入传入的日志内容（自动追加换行符） //
        fprintf(file, "%s\n", str.c_str());

        // 3. 必须关闭文件释放资源 //
        fclose(file);

        return true;
    }

    bool FileManagement::copyDir(const std::filesystem::path& sourcePath,
                                      const std::filesystem::path& targetPath)
    {
        try
        {
            // 检查源路径
            if (!std::filesystem::exists(sourcePath))
            {
                std::cerr << "Source path does not exist: " + sourcePath.string();
                return false;
            }
            if (!std::filesystem::is_directory(sourcePath))
            {
                std::cerr << "Source path is not a directory: " + sourcePath.string();
                return false;
            }

            // 在目标路径下创建同名文件夹
            auto targetDir = targetPath / sourcePath.filename();
            std::filesystem::create_directories(targetDir);

            // 递归复制内容
            for (const auto& entry : std::filesystem::directory_iterator(sourcePath))
            {
                const auto& path = entry.path();
                auto target = targetDir / path.filename();

                if (std::filesystem::is_directory(path))
                {
                    // 递归复制子目录
                    copyDir(path, targetDir);
                }
                else if (std::filesystem::is_regular_file(path))
                {
                    std::filesystem::copy_file(path, target, std::filesystem::copy_options::overwrite_existing);
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "File system error: " << e.what() << '\n';
            return false;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Standard exception: " << e.what() << '\n';
            return false;
        }
        return true;
    }

    std::string FileManagement::getFileTree(const std::string& rootPath)
    {
        std::string result;
        result.reserve(1024 * 100);

        // 1. 输出根目录路径，并统一替换为 '/'
        std::string displayRootPath = rootPath;
        std::replace(displayRootPath.begin(), displayRootPath.end(), '\\', '/');
        result += displayRootPath + "/\n";

        std::function<void(const std::filesystem::path&, int)> walkDir;
        walkDir = [&](const std::filesystem::path& dir, int depth) {
            try
            {
                for (const auto& entry : std::filesystem::directory_iterator(dir))
                {
                    auto filename = entry.path().filename().u8string();

                    // 2. 构建缩进和条目行
                    result += std::string(depth * 2, ' ') + filename;

                    if (std::filesystem::is_directory(entry.status()))
                    {
                        result += "/\n";
                        walkDir(entry.path(), depth + 1);
                    }
                    else
                    {
                        result += "\n";
                    }
                }
            }
            catch (...)
            {
            }
        };

        walkDir(std::filesystem::u8path(rootPath), 0);
        return result;
    }

    /******************** [第三方库] FileObject 文件目录对象 ********************/

    FileObject::FileObject()
            : rootDir(""),
              model(fileManagementCode::MatchMode::EXACT),
              caseSensitive(false)
    {
    }

    FileObject::FileObject(const std::string& path,
                           fileManagementCode::MatchMode mode,
                           bool cs)
            : rootDir(path),
              model(mode),
              caseSensitive(cs)
    {
        loadDirectory();
    }

    // 递归释放所有子目录对象 //
    FileObject::~FileObject()
    {
        for (auto* sub : subDirectories)
        {
            delete sub;
        }
        subDirectories.clear();
    }

    // 移动后清空源对象的子目录指针，避免重复释放 //
    FileObject::FileObject(FileObject&& other) noexcept
            : rootDir(std::move(other.rootDir)),
              fileNames(std::move(other.fileNames)),
              dirNames(std::move(other.dirNames)),
              subDirectories(std::move(other.subDirectories)),
              model(other.model),
              caseSensitive(other.caseSensitive)
    {
        other.subDirectories.clear();
    }

    void FileObject::setModel(fileManagementCode::MatchMode mode)
    {
        model = mode;
    }

    fileManagementCode::MatchMode FileObject::getModel() const
    {
        return model;
    }

    void FileObject::setCaseSensitive(bool cs)
    {
        caseSensitive = cs;
    }

    bool FileObject::isCaseSensitive() const
    {
        return caseSensitive;
    }

    // 根据配置统一大小写，供普通字符串和正则匹配共用 //
    std::string FileObject::_prepareString(const std::string& str) const
    {
        if (caseSensitive)
        {
            return str;
        }

        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::tolower(ch));
                       });
        return lower;
    }

    // 使用当前匹配模式判断单个文件名 // 
    bool FileObject::_matches(const std::string& fileName, const std::string& pattern) const
    {
        const std::string preparedFileName = _prepareString(fileName);
        const std::string preparedPattern = _prepareString(pattern);

        switch (model)
        {
            case fileManagementCode::MatchMode::EXACT:
                return preparedFileName == preparedPattern;

            case fileManagementCode::MatchMode::REGEX:
                try
                {
                    const std::regex re(
                            preparedPattern,
                            caseSensitive ? std::regex::ECMAScript : std::regex::icase);
                    return std::regex_search(preparedFileName, re);
                }
                catch (const std::regex_error&)
                {
                    return false;
                }

            case fileManagementCode::MatchMode::WHOLE_WORD:
                return preparedFileName == preparedPattern;

            case fileManagementCode::MatchMode::CONTAINS:
                return preparedFileName.find(preparedPattern) != std::string::npos;

            default:
                return false;
        }
    }

    // 递归收集当前目录和所有子目录中的文件完整路径 //
    void FileObject::_collectAllFiles(std::vector<std::string>& result) const
    {
        for (const auto& fileName : fileNames)
        {
            result.push_back((std::filesystem::path(rootDir) / fileName).string());
        }

        for (const auto* subDir : subDirectories)
        {
            subDir->_collectAllFiles(result);
        }
    }

    // 递归搜索文件名，并将命中的完整路径写入结果 //
    void FileObject::_searchRecursive(const std::string& pattern,
                                      std::vector<std::string>& results) const
    {
        for (const auto& fileName : fileNames)
        {
            if (_matches(fileName, pattern))
            {
                results.push_back((std::filesystem::path(rootDir) / fileName).string());
            }
        }

        for (const auto* subDir : subDirectories)
        {
            subDir->_searchRecursive(pattern, results);
        }
    }

    // 对外提供递归文件名搜索 //
    std::vector<std::string> FileObject::searchFiles(const std::string& pattern) const
    {
        std::vector<std::string> results;
        _searchRecursive(pattern, results);
        return results;
    }

    // 将枚举状态转换为稳定的英文描述 //
    std::string FileObject::translateModelToEnglish(fileManagementCode::MatchMode mode)
    {
        switch (mode)
        {
            case fileManagementCode::MatchMode::EXACT:
                return "Exact Match";
            case fileManagementCode::MatchMode::REGEX:
                return "Regular Expression";
            case fileManagementCode::MatchMode::WHOLE_WORD:
                return "Whole Word Match";
            case fileManagementCode::MatchMode::CONTAINS:
                return "Contains Match";
            default:
                return "Unknown Mode";
        }
    }

    // 将英文描述转换回枚举，无法识别时保持原类的 CONTAINS 默认行为 //
    fileManagementCode::MatchMode FileObject::translateFromEnglish(const std::string& englishDesc)
    {
        std::string lower = englishDesc;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::tolower(ch));
                       });

        if (lower == "exact match")
        {
            return fileManagementCode::MatchMode::EXACT;
        }
        if (lower == "regular expression")
        {
            return fileManagementCode::MatchMode::REGEX;
        }
        if (lower == "whole word match")
        {
            return fileManagementCode::MatchMode::WHOLE_WORD;
        }
        if (lower == "contains match")
        {
            return fileManagementCode::MatchMode::CONTAINS;
        }

        return fileManagementCode::MatchMode::CONTAINS;
    }

    // 重新扫描根目录。重复调用时先释放旧子树，避免残留和重复条目 //
    void FileObject::loadDirectory()
    {
        for (auto* sub : subDirectories)
        {
            delete sub;
        }
        subDirectories.clear();
        fileNames.clear();
        dirNames.clear();

        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(rootDir))
            {
                if (std::filesystem::is_regular_file(entry.path()))
                {
                    fileNames.push_back(entry.path().filename().string());
                }
                else if (std::filesystem::is_directory(entry.path()))
                {
                    dirNames.push_back(entry.path().filename().string());

                    auto* subDir = new FileObject(entry.path().string(), model, caseSensitive);
                    subDirectories.push_back(subDir);
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "Error accessing directory: " << e.what() << std::endl;
        }
    }

    // 按层级输出目录、文件夹和文件 //
    void FileObject::printTree(int depth) const
    {
        std::string dirName = std::filesystem::path(rootDir).filename().string();
        if (dirName.empty())
        {
            dirName = rootDir;
        }

        const std::string indent(depth * 2, ' ');
        std::cout << indent << dirName << std::endl;

        for (const auto& dir : dirNames)
        {
            std::cout << indent << "  " << dir << "/" << std::endl;
        }

        for (const auto& fileName : fileNames)
        {
            std::cout << indent << "  " << fileName << std::endl;
        }

        for (const auto* subDir : subDirectories)
        {
            subDir->printTree(depth + 1);
        }
    }

    // 返回目录树字符串，不直接产生控制台输出 //
    std::string FileObject::getTreeString(int depth) const
    {
        std::ostringstream oss;

        std::string dirName = std::filesystem::path(rootDir).filename().string();
        if (dirName.empty())
        {
            dirName = rootDir;
        }

        const std::string indent(depth * 2, ' ');
        oss << indent << dirName << "\n";

        for (const auto& dir : dirNames)
        {
            oss << indent << "  " << dir << "/\n";
        }

        for (const auto& fileName : fileNames)
        {
            oss << indent << "  " << fileName << "\n";
        }

        for (const auto* subDir : subDirectories)
        {
            oss << subDir->getTreeString(depth + 1);
        }

        return oss.str();
    }

    // 获取当前目录条目，文件夹优先并添加 "/" 后缀 //
    std::vector<std::string> FileObject::getAllEntries() const
    {
        std::vector<std::string> entries;
        entries.reserve(dirNames.size() + fileNames.size());

        for (const auto& dir : dirNames)
        {
            entries.push_back(dir + "/");
        }

        for (const auto& file : fileNames)
        {
            entries.push_back(file);
        }

        return entries;
    }

    // 获取当前目录及递归子目录中所有文件的完整路径 //
    std::vector<std::string> FileObject::getAllFiles() const
    {
        std::vector<std::string> allFiles;
        _collectAllFiles(allFiles);
        return allFiles;
    }
}
