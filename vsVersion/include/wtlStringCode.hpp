#pragma once
/**
 * wtlStringCode.hpp — 字符串编码检测与转换（C++17 / Windows）
 *
 * 仅暴露 StringCode 类接口，实现位于对应 .cpp。
 * 由 wtlOpen.hpp 等其它模块共同依赖，故独立成模块。
 */

#include <string>

using std::string;
using std::wstring;

namespace wtl
{
    /*************************** 通用接口 类 ***************************/
/*
 支持检测的编码类别有:
 1. utf8bom     - UTF-8 with BOM (EF BB BF)
 2. utf16le     - UTF-16 Little Endian (FF FE)
 3. utf16be     - UTF-16 Big Endian (FE FF)
 4. utf32le     - UTF-32 Little Endian (FF FE 00 00)
 5. utf32be     - UTF-32 Big Endian (00 00 FE FF)
 6. utf8        - UTF-8 without BOM
 7. ascii       - ASCII / US-ASCII (0x00-0x7F)
 8. gbk         - GBK编码 (中文扩展编码)
 9. gb2312      - GB2312编码 (GBK子集，简体中文)
10. big5        - Big5编码 (繁体中文)
11. latin1      - Latin-1 / ISO-8859-1 (西欧语言)
12. binary/other- 二进制或其他未知编码

 支持转化的编码类别有:
 1. utf8        - UTF-8 without BOM (通用推荐编码)
 2. utf8bom     - UTF-8 with BOM (Windows常用)
 3. utf16le     - UTF-16 Little Endian (Windows内部编码)
 4. utf16be     - UTF-16 Big Endian (网络传输/其他系统)
 5. utf16       - UTF-16 with BOM (自动选择字节序)
 6. gbk         - GBK编码 (Windows中文环境默认)
 7. big5        - Big5编码 (繁体中文环境)
 8. latin1      - Latin-1 / ISO-8859-1 (西欧语言)
 9. ascii       - ASCII编码 (仅支持0x00-0x7F字符)

 编码名称别名支持:
 - UTF-8: utf-8, utf8
 - UTF-8 with BOM: utf-8bom, utf8bom, utf-8 with bom
 - UTF-16 LE: utf-16le, utf16le, utf-16 le
 - UTF-16 BE: utf-16be, utf16be, utf-16 be
 - UTF-16: utf-16, utf16 (Windows默认LE, 其他默认BE)
 - 中文编码: gb2312 → gbk, gb18030 → gbk (统一处理)
 - Latin-1: iso-8859-1 → latin1
 - ASCII: us-ascii → ascii

 转换说明:
 1. 所有转换通过UTF-8作为中间编码进行: 源编码 → UTF-8 → 目标编码
 2. BOM自动处理: 源字符串BOM自动移除，目标编码根据需要添加BOM
 3. 字符集兼容性:
    - ASCII → UTF-8: 无损转换
    - GBK/Big5 ↔ UTF-8: 完整支持中文
    - Latin1 ↔ UTF-8: 西欧字符无损，扩展字符可能丢失
    - 非ASCII转ASCII: 非ASCII字符用'?'替换
 4. 平台特性:
    - Windows环境: 默认猜测GBK编码，UTF-16使用LE字节序
    - 其他环境: 默认猜测UTF-8编码，UTF-16使用BE字节序

 使用示例:
 std::string result = converter.stringChangeEncoding("文本", "utf8");
*/

    /*********************** 更改字符串编码 **************************/
    class StringCode
    {
    private:
        // 辅助函数
        std::string _convertBetweenEncodings(const std::string &str, const std::string &fromEncoding, const std::string &toEncoding);
        std::string _convertToUtf8(const std::string &str, const std::string &fromEncoding);
        std::string _convertFromUtf8(const std::string &str, const std::string &toEncoding);
        std::string _normalizeEncodingName(const std::string &encoding);
        bool _isUtf8Encoding(const std::string &encoding);
        bool _isUtf16Encoding(const std::string &encoding);
        bool _isUtf32Encoding(const std::string &encoding);
        std::string _removeBom(const std::string &str, std::string &detectedEncoding);
    public:
        std::string gbkToUtf8(const std::string& gbkStr);
        std::string utf8ToGbk(const std::string& utf8Str);
        std::wstring strToWStr(const std::string& str);
        std::wstring strToWStr(const std::string& str, bool useGBK);
        std::string wStrToStr(const std::wstring& wstr);
        /************* 老接口 *************/
        // 自动转码 //
        string stringChangeEncoding(const string &str, const string &targetEncoding);

        // 推导编码 //
        std::string checkStringCode(const std::string &str);
    };
}
