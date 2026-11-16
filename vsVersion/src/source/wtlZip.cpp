#include "wtlZip.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>
#include <unzip.h>

using std::string;
using std::vector;
using std::wstring;
using std::cout;
using std::cerr;
using std::endl;

namespace fs = std::filesystem;

/********************** 压缩器 **********************/
/******************** 压缩 ********************/
// 获取文件列表（递归） - 使用宽字符API版本
void wtl::ZipDirToBin::_getFileList(const fs::path &folderPathUtf8, const fs::path &basePathUtf8, vector<FileEntry>& fileList)
{
    // 使用宽字符路径（fs::path 内部已使用wstring）
    wstring searchPath = folderPathUtf8.wstring() + L"\\*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        // 错误输出改为英文，且不判断DEBUG宏
        cerr << "Error: Failed to open directory: " << folderPathUtf8.string() << endl;
        return;
    }

    do {
        // 跳过 "." 和 ".." 目录
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
        {
            continue;
        }

        // 构建完整路径
        fs::path fullPathWide = folderPathUtf8 / findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // 添加目录条目
            fs::path relativePathWide = fs::relative(fullPathWide, basePathUtf8);
            string relativePathUtf8 = relativePathWide.u8string();

            FileEntry dirEntry;
            dirEntry.relativePath = relativePathUtf8;
            dirEntry.isDirectory = TRUE;
            dirEntry.attributes = findData.dwFileAttributes;
            dirEntry.lastWriteTime = findData.ftLastWriteTime;
            dirEntry.fileSize = 0;  // 目录大小为0
            dirEntry.data.clear();  // 无数据

            fileList.push_back(dirEntry);

#ifdef DEBUG
            cout << "添加目录: " << dirEntry.relativePath << endl;
#endif

            // 递归处理子目录
            _getFileList(fullPathWide, basePathUtf8, fileList);
        }
        else
        {
            // 计算相对路径
            fs::path relativePathWide = fs::relative(fullPathWide, basePathUtf8);

            // 使用自动编码转换将宽字符路径转为UTF-8存储
            string relativePathUtf8 = relativePathWide.u8string();

            // 创建文件条目
            FileEntry entry;
            entry.relativePath = relativePathUtf8;  // 存储UTF-8编码的相对路径
            entry.isDirectory = FALSE;
            entry.attributes = findData.dwFileAttributes;
            entry.fileSize = (static_cast<uint64_t>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
            entry.lastWriteTime = findData.ftLastWriteTime;

            // 使用宽字符API打开文件
            HANDLE hFile = CreateFileW(
                    fullPathWide.wstring().c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
            );

            if (hFile != INVALID_HANDLE_VALUE)
            {
                // 读取文件内容
                entry.data.resize(entry.fileSize);
                DWORD bytesRead = 0;

                if (ReadFile(hFile, entry.data.data(), static_cast<DWORD>(entry.fileSize), &bytesRead, NULL))
                {
                    if (bytesRead == entry.fileSize)
                    {
                        fileList.push_back(entry);
#ifdef DEBUG
                        cout << "添加文件: " << entry.relativePath << " (" << entry.fileSize << " bytes)" << endl;
#endif
                    }
                    else
                    {
                        // 错误输出改为英文，且不判断DEBUG宏
                        cerr << "Warning: Incomplete file read: " << entry.relativePath
                             << " (Expected: " << entry.fileSize << ", Actual: " << bytesRead << ")" << endl;
                    }
                }
                else
                {
                    // 错误输出改为英文，且不判断DEBUG宏
                    DWORD error = GetLastError();
                    cerr << "Error: Failed to read file: " << entry.relativePath
                         << " (Error code: " << error << ")" << endl;
                }

                CloseHandle(hFile);
            }
            else
            {
                // 错误输出改为英文，且不判断DEBUG宏
                DWORD error = GetLastError();
                cerr << "Error: Failed to open file: " << fullPathWide.string()
                     << " (Error code: " << error << ")" << endl;
            }
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}
// 序列化文件列表为内存数据
vector<BYTE> wtl::ZipDirToBin::_serializeFileList(const vector<FileEntry>& fileList)
{
    std::stringstream ss;

    // 检查文件列表是否为空
    if (fileList.empty())
    {
        // 错误输出改为英文，且不判断DEBUG宏
        cerr << "Warning: File list is empty, returning empty data" << endl;
    }

    // 写入文件数量
    DWORD fileCount = fileList.size();
    ss.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

    // 调试输出文件数量
#ifdef DEBUG
    cout << "序列化文件数量: " << fileCount << endl;
#endif

    for (const auto& entry : fileList)
    {
        // 写入是否为目录
        BOOL isDir = entry.isDirectory;
        ss.write(reinterpret_cast<const char*>(&isDir), sizeof(isDir));

        // 写入属性
        ss.write(reinterpret_cast<const char*>(&entry.attributes), sizeof(entry.attributes));

        // 写入文件名长度和文件名（UTF-8）
        DWORD nameLength = static_cast<DWORD>(entry.relativePath.length());

        // 检查文件名长度是否合理
        if (nameLength == 0 || nameLength > MAX_PATH)
        {
            // 错误输出改为英文，且不判断DEBUG宏
            cerr << "Warning: Invalid filename length: " << nameLength
                 << " for entry, skipping" << endl;
            continue;
        }

        ss.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
        ss.write(entry.relativePath.c_str(), nameLength);

        // 调试输出
#ifdef DEBUG
        if (entry.isDirectory)
        {
            cout << "序列化目录: " << entry.relativePath
                 << " (长度: " << nameLength << ")" << endl;
        }
        else
        {
            cout << "序列化文件: " << entry.relativePath
                 << " (长度: " << nameLength << ")" << endl;
        }
#endif

        // 写入文件大小
        ss.write(reinterpret_cast<const char*>(&entry.fileSize), sizeof(entry.fileSize));

        // 检查文件大小是否与数据一致（仅对文件）
        if (!entry.isDirectory && entry.fileSize != entry.data.size())
        {
            // 错误输出改为英文，且不判断DEBUG宏
            cerr << "Warning: File size mismatch for " << entry.relativePath
                 << " (recorded: " << entry.fileSize
                 << ", actual: " << entry.data.size() << ")" << endl;
        }

        // 调试输出文件大小
#ifdef DEBUG
        if (!entry.isDirectory)
        {
            cout << "  文件大小: " << entry.fileSize << " bytes" << endl;
        }
#endif

        // 写入最后修改时间
        ss.write(reinterpret_cast<const char*>(&entry.lastWriteTime), sizeof(entry.lastWriteTime));

        // 检查文件数据是否为空（仅对文件）
        if (!entry.isDirectory)
        {
            if (entry.data.empty())
            {
                // 错误输出改为英文，且不判断DEBUG宏
                cerr << "Warning: Empty data for file: " << entry.relativePath << endl;
            }
            else
            {
                // 写入文件数据
                ss.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());

                // 调试输出数据大小
#ifdef DEBUG
                cout << "  数据大小: " << entry.data.size() << " bytes" << endl;
#endif
            }
        }
    }

    string str = ss.str();

    // 调试输出总序列化大小
#ifdef DEBUG
    cout << "总序列化大小: " << str.size() << " bytes" << endl;
#endif

    return vector<BYTE>(str.begin(), str.end());
}

// 使用zlib压缩数据 //
BOOL wtl::ZipDirToBin::_compressData(const vector<BYTE>& input, vector<BYTE>& output)
{
    uLongf destLen = compressBound(input.size());
    output.resize(destLen);

    int ret = compress(output.data(), &destLen, input.data(), input.size());
    if (ret == Z_OK)
    {
        output.resize(destLen);
        return TRUE;
    }
    return FALSE;
}

// 保存数据到文件
BOOL wtl::ZipDirToBin::_saveToFile(const fs::path& filenamePath, const vector<BYTE>& data)
{
    // 检查输入数据是否为空
    if (data.empty())
    {
        // 错误输出：使用英文，不判断DEBUG宏
        cerr << "Error: Data buffer is empty, nothing to save" << endl;
        return FALSE;
    }

    // 检查路径是否有效
    if (filenamePath.empty())
    {
        cerr << "Error: Filename path is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    // 使用自动编码转换输出文件名（调试信息保持中文）
    string filenameUtf8 = filenamePath.u8string();
    cout << "保存文件: " << filenameUtf8 << " (大小: " << data.size() << " bytes)" << endl;
#endif

    // 使用宽字符API创建文件
    HANDLE hFile = CreateFileW(
            filenamePath.wstring().c_str(),  // 宽字符路径
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,                    // 总是创建新文件，覆盖已存在的
            FILE_ATTRIBUTE_NORMAL,
            NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        cerr << "Error: Failed to create file: " << filenamePath.string()
             << " (Error code: " << error << ")" << endl;
        return FALSE;
    }

    DWORD bytesWritten = 0;
    BOOL result = FALSE;

    // 写入数据
    if (!WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, NULL))
    {
        DWORD error = GetLastError();
        cerr << "Error: Failed to write file: " << filenamePath.string()
             << " (Error code: " << error << ")" << endl;
    }
    else if (bytesWritten != data.size())
    {
        cerr << "Error: Incomplete write operation: " << filenamePath.string()
             << " (Expected: " << data.size() << " bytes, Written: " << bytesWritten << " bytes)" << endl;
    }
    else
    {
        result = TRUE;  // 写入成功
    }

    CloseHandle(hFile);

    // 如果写入失败，尝试删除不完整的文件
    if (!result && hFile != INVALID_HANDLE_VALUE)
    {
        DeleteFileW(filenamePath.wstring().c_str());
    }

    return result;
}

/******************** 解压 ********************/
/****************** 解压功能 ******************/
// 读取压缩文件到内存
BOOL wtl::ZipDirToBin::_readCompressedFile(const fs::path& filenamePath, vector<BYTE>& data)
{
    // 检查文件路径是否为空
    if (filenamePath.empty())
    {
        // 错误输出：使用英文，不判断DEBUG宏
        cerr << "Error: Filename path is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    // 使用自动编码转换输出文件名（调试信息保持中文）
    string filenameUtf8 = filenamePath.u8string();
    cout << "读取压缩文件: " << filenameUtf8 << endl;
#endif

    // 使用宽字符API打开文件
    HANDLE hFile = CreateFileW(
            filenamePath.wstring().c_str(),  // 宽字符路径
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,                    // 打开已存在的文件
            FILE_ATTRIBUTE_NORMAL,
            NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        cerr << "Error: Failed to open file: " << filenamePath.string()
             << " (Error code: " << error << ")" << endl;
        return FALSE;
    }

    // 获取文件大小
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE)
    {
        DWORD error = GetLastError();
        cerr << "Error: Failed to get file size: " << filenamePath.string()
             << " (Error code: " << error << ")" << endl;
        CloseHandle(hFile);
        return FALSE;
    }

#ifdef DEBUG
    cout << "文件大小: " << fileSize << " bytes" << endl;
#endif

    // 如果文件大小为0，直接返回成功
    if (fileSize == 0)
    {
#ifdef DEBUG
        cout << "文件为空，直接返回" << endl;
#endif
        data.clear();
        CloseHandle(hFile);
        return TRUE;
    }

    // 调整数据缓冲区大小
    data.resize(fileSize);

    DWORD bytesRead = 0;
    BOOL result = ReadFile(hFile, data.data(), fileSize, &bytesRead, NULL);

    // 检查读取结果
    if (!result)
    {
        DWORD error = GetLastError();
        cerr << "Error: Failed to read file: " << filenamePath.string()
             << " (Error code: " << error << ")" << endl;
    }
    else if (bytesRead != fileSize)
    {
        cerr << "Error: Incomplete file read: " << filenamePath.string()
             << " (Expected: " << fileSize << " bytes, Read: " << bytesRead << " bytes)" << endl;
        result = FALSE;  // 标记为失败
    }
    else
    {
#ifdef DEBUG
        cout << "文件读取成功: " << bytesRead << " bytes" << endl;
#endif
    }

    CloseHandle(hFile);

    // 如果读取失败，清空数据缓冲区
    if (!result)
    {
        data.clear();
    }

    return result;
}

// 反序列化文件列表
BOOL wtl::ZipDirToBin::_deserializeFileList(const vector<BYTE>& data, vector<FileEntry>& fileList)
{
    if (data.size() < sizeof(DWORD)) return FALSE;

    const BYTE* ptr = data.data();
    DWORD fileCount = *reinterpret_cast<const DWORD*>(ptr);
    ptr += sizeof(DWORD);

    for (DWORD i = 0; i < fileCount; i++)
    {
        FileEntry entry;

        // 读取是否为目录
        if (ptr + sizeof(BOOL) > data.data() + data.size()) return FALSE;
        entry.isDirectory = *reinterpret_cast<const BOOL*>(ptr);
        ptr += sizeof(BOOL);

        // 读取属性
        if (ptr + sizeof(DWORD) > data.data() + data.size()) return FALSE;
        entry.attributes = *reinterpret_cast<const DWORD*>(ptr);
        ptr += sizeof(DWORD);

        // 读取文件名长度和文件名（UTF-8）
        if (ptr + sizeof(DWORD) > data.data() + data.size()) return FALSE;
        DWORD nameLength = *reinterpret_cast<const DWORD*>(ptr);
        ptr += sizeof(DWORD);

        if (ptr + nameLength > data.data() + data.size()) return FALSE;
        entry.relativePath.assign(reinterpret_cast<const char*>(ptr), nameLength);
        ptr += nameLength;

        // 读取文件大小
        if (ptr + sizeof(DWORD) > data.data() + data.size()) return FALSE;
        entry.fileSize = *reinterpret_cast<const DWORD*>(ptr);
        ptr += sizeof(DWORD);

        // 读取最后修改时间
        if (ptr + sizeof(FILETIME) > data.data() + data.size()) return FALSE;
        entry.lastWriteTime = *reinterpret_cast<const FILETIME*>(ptr);
        ptr += sizeof(FILETIME);

        // 如果是文件，读取文件数据
        if (!entry.isDirectory)
        {
            if (ptr + entry.fileSize > data.data() + data.size()) return FALSE;
            entry.data.assign(ptr, ptr + entry.fileSize);
            ptr += entry.fileSize;
        }
        else
        {
            // 目录没有数据
            entry.data.clear();
        }

        fileList.push_back(entry);
    }

    return TRUE;
}


// 使用zlib解压数据 //
static BOOL _decompressData(const vector<BYTE>& compressedData, vector<BYTE>& decompressedData, uLongf originalSize)
{
    decompressedData.resize(originalSize);

    int ret = uncompress(decompressedData.data(), &originalSize,
                         compressedData.data(), compressedData.size());
    if (ret == Z_OK) {
        decompressedData.resize(originalSize);
        return TRUE;
    }
    return FALSE;
}

// 创建目录（递归创建）
BOOL wtl::ZipDirToBin::_createDirectoryRecursive(const fs::path& path)
{
    // 检查路径是否为空
    if (path.empty())
    {
        cerr << "Error: Directory path is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    // 使用自动编码转换输出路径（调试信息保持中文）
    string pathUtf8 = path.u8string();
    cout << "创建目录: " << pathUtf8 << endl;
#endif

    // 检查目录是否已存在
    DWORD attr = GetFileAttributesW(path.wstring().c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
#ifdef DEBUG
        cout << "目录已存在: " << pathUtf8 << endl;
#endif
        return TRUE;
    }

#ifdef DEBUG
    // 检查路径是否存在但不是一个目录
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        cout << "警告: 路径存在但不是目录: " << pathUtf8 << endl;
    }
#endif

    // 获取父目录
    fs::path parentPath = path.parent_path();

    // 递归创建父目录（如果父目录不为空）
    if (!parentPath.empty() && parentPath != path.root_path() && parentPath != path.root_name())
    {
#ifdef DEBUG
        string parentUtf8 = parentPath.u8string();
        cout << "创建父目录: " << parentUtf8 << endl;
#endif

        if (!_createDirectoryRecursive(parentPath))
        {
            cerr << "Error: Failed to create parent directory: " << parentPath.string() << endl;
            return FALSE;
        }
    }

    // 创建当前目录
    if (CreateDirectoryW(path.wstring().c_str(), NULL))
    {
#ifdef DEBUG
        cout << "目录创建成功: " << pathUtf8 << endl;
#endif
        return TRUE;
    }
    else
    {
        DWORD error = GetLastError();

        // 检查是否因为目录已存在而失败（多线程竞争条件）
        if (error == ERROR_ALREADY_EXISTS)
        {
#ifdef DEBUG
            cout << "目录已被其他进程创建: " << pathUtf8 << endl;
#endif
            return TRUE;
        }

        cerr << "Error: Failed to create directory: " << path.string()
             << " (Error code: " << error << ")" << endl;
        return FALSE;
    }
}

// 恢复文件 //
BOOL wtl::ZipDirToBin::_restoreFile(const FileEntry& entry, const fs::path& basePath)
{
    // 检查基本路径是否为空
    if (basePath.empty())
    {
        cerr << "Error: Base path is empty" << endl;
        return FALSE;
    }

    // 检查文件条目是否有效
    if (entry.relativePath.empty())
    {
        cerr << "Error: File entry has empty relative path" << endl;
        return FALSE;
    }

    // 构建完整路径
    fs::path fullPath = basePath / fs::path(entry.relativePath);

#ifdef DEBUG
    string fullPathUtf8 = fullPath.u8string();
    if (entry.isDirectory)
    {
        cout << "恢复目录: " << entry.relativePath << endl;
    }
    else
    {
        cout << "恢复文件: " << entry.relativePath << " (" << entry.data.size() << " bytes)" << endl;
    }
    cout << "目标路径: " << fullPathUtf8 << endl;
#endif

    if (entry.isDirectory)
    {
        // 处理目录
        if (!_createDirectoryRecursive(fullPath))
        {
            cerr << "Error: Failed to create directory: " << entry.relativePath << endl;
            return FALSE;
        }

        // 设置目录属性
        if (entry.attributes != 0)
        {
            SetFileAttributesW(fullPath.wstring().c_str(), entry.attributes);
        }

        // 设置目录时间
        HANDLE hDir = CreateFileW(fullPath.wstring().c_str(),
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS,
                                  NULL);
        if (hDir != INVALID_HANDLE_VALUE)
        {
            SetFileTime(hDir, NULL, NULL, &entry.lastWriteTime);
            CloseHandle(hDir);
        }

        return TRUE;
    }
    else
    {
        // 创建文件所在目录
        fs::path dirPath = fullPath.parent_path();
        if (!dirPath.empty())
        {
            if (!_createDirectoryRecursive(dirPath))
            {
                cerr << "Error: Failed to create directory for file: " << entry.relativePath << endl;
                return FALSE;
            }
        }

        // 使用宽字符API创建文件
        HANDLE hFile = CreateFileW(
                fullPath.wstring().c_str(),  // 宽字符路径
                GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,                // 总是创建新文件，覆盖已存在的
                FILE_ATTRIBUTE_NORMAL,
                NULL
        );

        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();
            cerr << "Error: Failed to create file: " << entry.relativePath
                 << " (Error code: " << error << ")" << endl;
            return FALSE;
        }

        DWORD bytesWritten = 0;
        BOOL result = FALSE;

        // 写入文件数据
        if (!WriteFile(hFile, entry.data.data(), static_cast<DWORD>(entry.data.size()), &bytesWritten, NULL))
        {
            DWORD error = GetLastError();
            cerr << "Error: Failed to write file: " << entry.relativePath
                 << " (Error code: " << error << ")" << endl;
        }
        else if (bytesWritten != entry.data.size())
        {
            cerr << "Error: Incomplete file write: " << entry.relativePath
                 << " (Expected: " << entry.data.size() << " bytes, Written: " << bytesWritten << " bytes)" << endl;
        }
        else
        {
            result = TRUE;  // 写入成功
        }

        // 设置文件时间和属性
        if (result)
        {
            if (!SetFileTime(hFile, NULL, NULL, &entry.lastWriteTime))
            {
                DWORD error = GetLastError();
                cerr << "Warning: Failed to set file time for: " << entry.relativePath
                     << " (Error code: " << error << ")" << endl;
            }

            if (entry.attributes != 0)
            {
                SetFileAttributesW(fullPath.wstring().c_str(), entry.attributes);
            }

#ifdef DEBUG
            cout << "文件时间已设置: " << entry.relativePath << endl;
#endif
        }

        CloseHandle(hFile);

        // 如果写入失败，尝试删除不完整的文件
        if (!result && hFile != INVALID_HANDLE_VALUE)
        {
            DeleteFileW(fullPath.wstring().c_str());
        }

        if (result)
        {
#ifdef DEBUG
            cout << "文件恢复成功: " << entry.relativePath
                 << " (" << bytesWritten << " bytes)" << endl;
#endif
            return TRUE;
        }

        return FALSE;
    }
}

/****************** 接口暴露 ******************/

// 压缩文件夹
bool wtl::ZipDirToBin::compressFolder(const fs::path& folderPath, const fs::path& outputFile)
{
    // 检查输入路径
    if (folderPath.empty() || outputFile.empty())
    {
        cerr << "Error: Folder path or output file path is empty" << endl;
        return FALSE;
    }

    // 检查是否为目录
    if (!_isDirectory(folderPath))
    {
        cerr << "Error: Path is not a directory: " << folderPath.string() << endl;
        return FALSE;
    }

#ifdef DEBUG
    string folderPathUtf8 = folderPath.u8string();
    string outputFileUtf8 = outputFile.u8string();
    cout << "开始压缩文件夹: " << folderPathUtf8 << endl;
    cout << "输出文件: " << outputFileUtf8 << endl;
#endif

    // 获取所有文件列表
    vector<FileEntry> fileList;
    _getFileList(folderPath, folderPath, fileList);

    if (fileList.empty())
    {
        cerr << "Error: Folder is empty or no files can be read" << endl;
        return FALSE;
    }

#ifdef DEBUG
    cout << "找到 " << fileList.size() << " 个文件" << endl;
#endif

    // 序列化文件数据
    vector<BYTE> serializedData = _serializeFileList(fileList);

    if (serializedData.empty())
    {
        cerr << "Error: Serialized data is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    cout << "序列化数据大小: " << serializedData.size() << " bytes" << endl;
#endif

    // 压缩数据
    vector<BYTE> compressedData;
    if (!_compressData(serializedData, compressedData))
    {
        // _compressData 函数内部已经有错误输出
        return FALSE;
    }

    if (compressedData.empty())
    {
        cerr << "Error: Compressed data is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    cout << "压缩后数据大小: " << compressedData.size() << " bytes" << endl;
    if (!serializedData.empty())
    {
        float compressionRatio = (compressedData.size() * 100.0f) / serializedData.size();
        cout << "压缩率: " << compressionRatio << "%" << endl;
    }
#endif

    // 保存压缩文件
    if (!_saveToFile(outputFile, compressedData))
    {
        // _saveToFile 函数内部已经有错误输出
        return FALSE;
    }

#ifdef DEBUG
    cout << "文件夹压缩完成" << endl;
#endif

    return TRUE;
}

// 解压文件夹
bool wtl::ZipDirToBin::decompressFolder(const fs::path& compressedFile, const fs::path& outputFolder)
{
    // 检查输入路径
    if (compressedFile.empty() || outputFolder.empty())
    {
        cerr << "Error: Compressed file path or output folder path is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    string compressedFileUtf8 = compressedFile.u8string();
    string outputFolderUtf8 = outputFolder.u8string();
    cout << "开始解压文件: " << compressedFileUtf8 << endl;
    cout << "解压到: " << outputFolderUtf8 << endl;
#endif

    // 读取压缩文件
    vector<BYTE> compressedData;
    if (!_readCompressedFile(compressedFile, compressedData))
    {
        // _readCompressedFile 函数内部已经有错误输出
        return FALSE;
    }

    if (compressedData.empty())
    {
        cerr << "Error: Compressed data is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    cout << "读取压缩数据大小: " << compressedData.size() << " bytes" << endl;
#endif

    // 估算原始大小（使用压缩率的经验值）
    uLongf estimatedSize = compressedData.size() * 10; // 假设压缩率为10:1

#ifdef DEBUG
    cout << "估算原始大小: " << estimatedSize << " bytes" << endl;
#endif

    // 解压数据
    vector<BYTE> serializedData;
    if (!_decompressData(compressedData, serializedData, estimatedSize))
    {
        // 如果第一次解压失败，尝试更大的缓冲区
        cerr << "Warning: First decompression attempt failed, trying larger buffer..." << endl;

        estimatedSize = compressedData.size() * 100; // 尝试更大的缓冲区
#ifdef DEBUG
        cout << "重试估算原始大小: " << estimatedSize << " bytes" << endl;
#endif

        if (!_decompressData(compressedData, serializedData, estimatedSize))
        {
            cerr << "Error: Data decompression failed after retry" << endl;
            return FALSE;
        }
    }

    if (serializedData.empty())
    {
        cerr << "Error: Decompressed data is empty" << endl;
        return FALSE;
    }

#ifdef DEBUG
    cout << "解压后数据大小: " << serializedData.size() << " bytes" << endl;
#endif

    // 反序列化文件列表
    vector<FileEntry> fileList;
    if (!_deserializeFileList(serializedData, fileList))
    {
        cerr << "Error: Failed to deserialize file list" << endl;
        return FALSE;
    }

    if (fileList.empty())
    {
        cerr << "Error: No files found in compressed data" << endl;
        return FALSE;
    }

#ifdef DEBUG
    cout << "找到 " << fileList.size() << " 个文件需要恢复" << endl;
#endif

    // 创建输出目录
    if (!_createDirectoryRecursive(outputFolder))
    {
        // _createDirectoryRecursive 函数内部已经有错误输出
        return FALSE;
    }

    // 恢复所有文件
    int successCount = 0;
    int failCount = 0;

    for (const auto& entry : fileList)
    {
        if (_restoreFile(entry, outputFolder))
        {
            successCount++;
        }
        else
        {
            failCount++;
#ifdef DEBUG
            cerr << "恢复文件失败: " << entry.relativePath << endl;
#endif
        }
    }

    // 输出结果统计
#ifdef DEBUG
    cout << "解压完成" << endl;
    cout << "成功恢复: " << successCount << " 个文件" << endl;
    if (failCount > 0)
    {
        cout << "失败: " << failCount << " 个文件" << endl;
    }
#endif

    if (failCount > 0)
    {
        cerr << "Warning: " << failCount << " file(s) failed to restore" << endl;
    }

    return (failCount == 0);
}
