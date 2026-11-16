#pragma once
/**
 * wtlOpen.hpp — 文件读写库（C++17 / Windows）
 *
 * 仅暴露 Open 类接口，非模板实现位于 src/source/wtlOpen.cpp，
 * 模板实现（writeJson）因需随类可见而保留在本头文件末尾。
 *
 * 使用：
 *   wtl::Open f("a.txt", "w");
 *   f.write("hello");
 */

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <wtlDict.hpp>
#include <wtlStringCode.hpp>

namespace fs = std::filesystem;

using std::string;
using std::vector;

namespace wtl
{
    // Open类读写文件 //
    /**
     open函数有r,rb,w,wb,a等模式读写文件，并且默认自动关闭文件，在离开作用域之后，带bin的函数需要以
     rb/wb/ab进行读取与写入，反之之间使用正常的w，b，a即可
     **/
    class Open
    {
    private:
        // 读写文件 //
        std::ifstream ipt;
        std::ofstream opt;
        // 文件模式 //
        fs::path filePath;
        string model;
        string return_encoding;

        bool ipt_isOpen = false;
        bool opt_isOpen = false;
        bool autoCloseFile = true;

        // 简单的 XOR 加密密钥 //
        const std::vector<unsigned char> encryptionKey = {
                0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90,
                0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B
        };

        StringCode scd;
    private:

        // XOR 加密/解密函数
        inline void _xorEncryptDecrypt(std::vector<unsigned char>& data)
        {
            size_t keyLen = encryptionKey.size();
            for (size_t i = 0; i < data.size(); ++i) {
                // 使用简单的XOR加密，避免复杂的变换
                data[i] ^= encryptionKey[i % keyLen];
            }
        }

        // 函数：将字符串转换为二进制数据（vector<unsigned char>） //
        void _stringToBinary(const std::string& str, std::vector<unsigned char>& binData);
        // 函数：将二进制数据写入文件（以二进制模式） //
        void _writeBinaryToFile(const std::vector<unsigned char>& binData, const std::string& filename);
        // 函数：从文件读取二进制数据 //
        void _readBinaryFromFile(const std::string& filename, std::vector<unsigned char>& binData);
        // 函数：将二进制数据转换回字符串（用于验证 //）
        void _binaryToString(const std::vector<unsigned char>& binData, std::string& str);

    public:
        // 默认构造函数 //
        Open() = default;

        Open(const string &file_path, const string &file_mode, const string &encoding="auto", bool auto_close=true);
        // 打开文件 //
        void open(const string &file_path, const string &file_mode, const string &encoding="auto", bool auto_close=true);

        // 检查是否成功打开 //
        bool isOpen();

        // 读取文件 //
        string read();
        vector<string> readLine();
        // 二进制读取文件 //
        std::vector<unsigned char> readBinary();
        // 读取json //
        Dict<string, JsonValue> readJson();
        // 二进制文件转字符串 //
        string readBinaryToString();

        // 写入文件 //
        void write(const string &str);
        // 写入二进制文件 //
        void writeBinary(const std::vector<unsigned char> &data);
        // 写入json //
        template<class K, class V>
        void writeJson(Dict<K, V> dicts);
        // 字符串转二进制 //
        void writeStringToBinary(const string &str);


        // 关闭文件 //
        void close();

        ~Open();
    };
}

/************** Open类头文件实现 **************/
// 写入json //
template <class K,class V>
void wtl::Open::writeJson(Dict<K, V> dicts)
{
    string dictString;
    if (return_encoding == "auto")
    {
        string stringCode = scd.checkStringCode(dicts.dictToString());
        dictString = scd.stringChangeEncoding(dicts.dictToString(), stringCode);
    }
    else
    {
        dictString = scd.stringChangeEncoding(dicts.dictToString(), return_encoding);
    }

//    string dictString = dicts.dictToString();
    opt<<dictString;
}
