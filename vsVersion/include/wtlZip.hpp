#pragma once
/**
 * wtlZip.hpp — 文件夹压缩/解压模块（C++17 / Windows）
 *
 * 仅暴露 ZipDirToBin 类接口，实现位于 src/source/wtlZip.cpp。
 *
 * 使用：
 *   wtl::ZipDirToBin zip;
 *   zip.compressFolder("C:/MyFolder", "archive.bin");
 */

#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using std::string;
using std::vector;

namespace wtl
{
    /********************** 压缩器--压缩文件夹 **********************/
    class ZipDirToBin
    {
    private:
        // 文件信息结构体
        struct FileEntry
        {
            string relativePath;      // UTF-8编码
            vector<BYTE> data;        // 文件数据
            DWORD fileSize;           // 文件大小
            FILETIME lastWriteTime;   // 最后修改时间
            BOOL isDirectory;         // 是否为目录
            DWORD attributes;         // 文件/目录属性
        };

    private:
        /******** 压缩 ********/
        // 检查是否为目录
        inline bool _isDirectory(const fs::path& getPath)
        {
            DWORD attr = GetFileAttributesW(getPath.wstring().c_str());
            return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
        }

        // 获取文件列表（递归）- 使用宽字符路径
        void _getFileList(const fs::path& folderPath, const fs::path& basePath, vector<FileEntry>& fileList);

        // 序列化文件列表为内存数据
        vector<BYTE> _serializeFileList(const vector<FileEntry>& fileList);

        // 使用zlib压缩数据
        BOOL _compressData(const vector<BYTE>& input, vector<BYTE>& output);

        // 保存数据到文件
        BOOL _saveToFile(const fs::path& filenamePath, const vector<BYTE>& data);

        /********* 解压 *********/
        // 读取压缩文件到内存
        BOOL _readCompressedFile(const fs::path& filenamePath, vector<BYTE>& data);

        BOOL _deserializeFileList(const vector<BYTE>& data, vector<FileEntry>& fileList);

        // 创建目录（递归创建）
        BOOL _createDirectoryRecursive(const fs::path& path);

        // 恢复文件
        BOOL _restoreFile(const FileEntry& entry, const fs::path& basePath);

    public:
        ZipDirToBin() = default;

        // 压缩文件夹
        bool compressFolder(const fs::path& folderPath, const fs::path& outputFile);

        // 解压文件夹
        bool decompressFolder(const fs::path& compressedFile, const fs::path& outputFolder);
    };
}
