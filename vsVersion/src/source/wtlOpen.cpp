#include "wtlOpen.hpp"
#include <iostream>

namespace fs = std::filesystem;

static wtl::StringCode scd_cpp;

/************** open类 ****************/
wtl::Open::Open(const string& file_path, const string& file_mode, const string &encoding, bool auto_close)
{
    open(file_path, file_mode, encoding, auto_close);
}

// 打开文件 //
void wtl::Open::open(const string& file_path, const string& file_mode, const string &encoding, bool auto_close)
{
    // 获取是否需要自动关闭文件，作用域退出后 //
    autoCloseFile = auto_close;

    // UTF-8 路径 -> filesystem::path（内部宽字符），无需 GBK 转码 //
    filePath = fs::u8path(file_path);

    model = file_mode;

    return_encoding = encoding;

    // 模式列表 //
    string model_list[] = { "r", "w", "a", "rb", "wb", "ab" };
    // 读 //
    if (model == model_list[0])
    {
        ipt.open(filePath);
        ipt_isOpen = ipt.is_open();
    }
        // 写 //
    else if (model == model_list[1])
    {
        opt.open(filePath);
        opt_isOpen = opt.is_open();
    }
        // 追加 //
    else if (model == model_list[2])
    {
        opt.open(filePath, std::ios::app);
        opt_isOpen = opt.is_open();
    }
        // 二进制读 //
    else if (model == model_list[3])
    {
        ipt.open(filePath, std::ios::binary);
        ipt_isOpen = ipt.is_open();
    }
        // 二进制携入 //
    else if (model == model_list[4])
    {
        opt.open(filePath, std::ios::binary);
        opt_isOpen = opt.is_open();
    }
        // 二进制追加 //
    else if (model == model_list[5])
    {
        opt.open(filePath, std::ios::app | std::ios::binary);
        opt_isOpen = opt.is_open();
    }
}

// 返回打开状态 //
bool wtl::Open::isOpen()
{
    // 模式列表 //
    string writeModelList[] = { "w", "a", "wb", "ab" };
    string readModelList[] = { "r", "rb" };
    // 遍历读功能列表返回 读取 文件打开状况 //
    for (auto& i : readModelList)
    {
        if (i == model)
        {
            return ipt_isOpen;
        }
    }
    // 遍历写文件列表 写入 写打开状态 //
    for (auto& i : writeModelList)
    {
        if (i == model)
        {
            return opt_isOpen;
        }
    }
    return false;
}

// 读取文件 //
string wtl::Open::read()
{
    string temp;
    string allLine;

    while (std::getline(ipt, temp))
    {
        allLine += (temp + "\n");
    }

    // 自动编码转化 //
    string endString;
    if (return_encoding == "auto")
    {
        string stringCode = scd.checkStringCode(allLine);
        endString = scd.stringChangeEncoding(allLine, stringCode);
    }
    else
    {
        endString = scd.stringChangeEncoding(allLine, return_encoding);
    }
    return endString;
}

wtl::Dict<string, wtl::JsonValue> wtl::Open::readJson()
{
    string temp = read();
    string jsonStr;

    if (return_encoding == "auto") {
        string stringCode = scd.checkStringCode(temp);
        jsonStr = scd.stringChangeEncoding(temp, stringCode);
    } else {
        jsonStr = scd.stringChangeEncoding(temp, return_encoding);
    }

    // 使用新的解析器
    StringToDictClass parser;
    wtl::JsonValue root = parser._parseJson(jsonStr);

    if (root.type == wtl::JsonValue::Object) {
        return root.objVal;
    }

    return Dict<string, wtl::JsonValue>();
}

// 读取json //
//wtl::Dict<string, string> wtl::Open::readJson()
//{
//    string jsonStr = read();
//    Dict<string, string> dct = wtl::stringToJson(jsonStr);
//    return dct;
//}

// 主解析函数 //
// 主解析函数 //
wtl::Dict<string, wtl::JsonValue> wtl::stringToJson(const std::string& jsonStr)
{
    StringToDictClass parser;
    std::string cleanedStr = jsonStr;

    // 去除首尾空白
    cleanedStr = parser._trim(cleanedStr);

    // 递归解析 JSON
    wtl::JsonValue result = parser._parseJson(cleanedStr);

    if (result.type == wtl::JsonValue::Object) {
        return result.objVal;
    }

    return Dict<string, wtl::JsonValue>();
}

// 二进制文件转字符串 //
string wtl::Open::readBinaryToString()
{
    std::string result;

    // 移除了文件打开状态检查，让用户自己通过 isOpen() 检查

    // 直接读取到文件末尾
    while (ipt.good()) {
        // 读取数据长度
        uint32_t dataSize = 0;
        ipt.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

        // 如果读取失败或数据大小为0，继续尝试而不是直接退出
        if (ipt.gcount() != sizeof(dataSize) || dataSize == 0) {
            continue;
        }

        // 读取加密数据
        std::vector<unsigned char> encryptedData(dataSize);
        ipt.read(reinterpret_cast<char*>(encryptedData.data()), dataSize);

        // 即使读取不完整也继续处理，而不是直接退出
        if (ipt.gcount() != static_cast<std::streamsize>(dataSize)) {
            // 调整数据大小为实际读取的大小
            encryptedData.resize(ipt.gcount());
        }

        // 解密数据
        _xorEncryptDecrypt(encryptedData);

        // 转换为字符串并追加到结果
        if (!encryptedData.empty()) {
            std::string segment(encryptedData.begin(), encryptedData.end());
            result += segment;
        }
    }

    return result;
}


vector<string> wtl::Open::readLine()
{
    string temp;
    vector<string> getLineStr;

    while (std::getline(ipt, temp))
    {
        // 自动编码转化 //
        string tempCoding;

        if (return_encoding == "auto")
        {
            string stringCode = scd.checkStringCode(temp);
            tempCoding = scd.stringChangeEncoding(temp, stringCode);
        }
        else
        {
            tempCoding = scd.stringChangeEncoding(temp, return_encoding);
        }
        getLineStr.emplace_back(tempCoding);
    }
    return getLineStr;
}


// 写入文件 //
void wtl::Open::write(const string& str)
{
    string endStringCode;
    if (return_encoding == "auto")
    {
        string stringCode = scd.checkStringCode(str);
        endStringCode = scd.stringChangeEncoding(str, stringCode);
    }
    else
    {
        endStringCode = scd.stringChangeEncoding(str, return_encoding);
    }
    opt << endStringCode;
}

// 二进制读取文件 //
std::vector<unsigned char> wtl::Open::readBinary()
{
    std::vector<unsigned char> fileData;

    // 检查文件是否成功打开
    if (!ipt.is_open()) {
        std::cerr << "Error: File is not open for reading" << std::endl;
        return fileData;
    }

    // 保存当前文件指针位置
    auto currentPos = ipt.tellg();

    // 移动到文件末尾获取文件大小
    ipt.seekg(0, std::ios::end);
    auto fileSize = ipt.tellg();

    if (fileSize <= 0) {
        std::cerr << "Error: File is empty or could not determine size" << std::endl;
        ipt.seekg(currentPos); // 恢复原位置
        return fileData;
    }

    // 移回文件开头
    ipt.seekg(0, std::ios::beg);

    // 调整vector大小以容纳整个文件
    fileData.resize(fileSize);

    // 读取文件数据
    if (!ipt.read(reinterpret_cast<char*>(fileData.data()), fileSize)) {
        std::cerr << "Error: Failed to read the entire file contents" << std::endl;
        fileData.clear(); // 读取失败时清空数据
    }

    // 恢复原文件指针位置（可选）
    ipt.seekg(currentPos);

    return fileData;
}

void wtl::Open::writeBinary(const std::vector<unsigned char>& data)
{
    // 直接写入数据，不添加额外信息
    if (!data.empty())
    {
        opt.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    if (!opt.good())
    {
        std::cerr << "Error: An error occurred while writing";
    }
}


// 字符串转二进制 //
void wtl::Open::writeStringToBinary(const string& str)
{
    if (!opt.is_open()) {
        std::cerr << "File is not open for writing" << std::endl;
        return;
    }

    std::vector<unsigned char> binData;
    _stringToBinary(str, binData);

    // 写入数据长度（4字节）
    uint32_t dataSize = static_cast<uint32_t>(binData.size());
    opt.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));

    // 写入实际数据
    if (!binData.empty()) {
        opt.write(reinterpret_cast<const char*>(binData.data()), binData.size());
    }

    // 确保数据写入
    opt.flush();
}

/***************** Open 辅助函数 *****************/
// 函数：将字符串转换为二进制数据（vector<unsigned char>） //
void wtl::Open::_stringToBinary(const std::string& str, std::vector<unsigned char>& binData)
{
    // 直接转换，不添加校验头
    binData.assign(str.begin(), str.end());

    // 使用更稳定的加密方式
    _xorEncryptDecrypt(binData);
}

// 函数：将二进制数据写入文件（以二进制模式） //
void wtl::Open::_writeBinaryToFile(const std::vector<unsigned char>& binData, const std::string& filename)
{
    opt.write(reinterpret_cast<const char*>(binData.data()), binData.size());
}

// 函数：从文件读取二进制数据 //
void wtl::Open::_readBinaryFromFile(const std::string& filename, std::vector<unsigned char>& binData)
{
    // 获取文件大小
    ipt.seekg(0, std::ios::end);
    std::streamsize fileSize = ipt.tellg();
    ipt.seekg(0, std::ios::beg);

    // 调整vector大小以容纳文件数据
    binData.resize(fileSize);

    // 读取数据到vector中
    ipt.read(reinterpret_cast<char*>(binData.data()), fileSize);
}

// 函数：将二进制数据转换回字符串（用于验证 //）
void wtl::Open::_binaryToString(const std::vector<unsigned char>& binData, std::string& str)
{
    if (binData.size() < 2) {
        str.clear();
        return;
    }

    std::vector<unsigned char> decryptedData = binData;
    _xorEncryptDecrypt(decryptedData); // 先解密

    // 移除校验头（如果添加了）
    if (decryptedData.size() >= 2) {
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + 2);
    }

    str.assign(decryptedData.begin(), decryptedData.end());
}


// 关闭文件 //
void wtl::Open::close()
{
    if (ipt.is_open())
    {
        ipt.close();
    }

    if (opt.is_open())
    {
        opt.close();
    }
}

wtl::Open::~Open()
{
    if (autoCloseFile)
    {
        // 输入关闭 //
        if (ipt.is_open())
        {
            ipt.close();
        }
        // 输出关闭 //
        if (opt.is_open())
        {
            opt.close();
        }
    }
}
