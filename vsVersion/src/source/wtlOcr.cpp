
#include <winToolsH.hpp>
// Tesseract & Leptonica — 仅cpp内部使用, 不暴露给库的使用者
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>


#include <cstdio>
#include <cstdlib>
#include <filesystem>

wtl::StringCode scd;

namespace auxiliaryTools
{
    // 自动查找tessdata路径
    // 只做路径搜索, 不设置环境变量, 不附带任何默认搜索路径
    // 使用 fs::path 处理宽字符/Unicode路径
    // 检查目录中是否存在任意 .traineddata 文件
    static bool _hasTraineddata(const std::filesystem::path& dir)
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.path().extension() == ".traineddata")
                return true;
        }
        return false;
    }

}

namespace ocr {
    namespace {

        // ═══════════════════════════════════════════════════════════════
        // 内部辅助函数 (自由函数, 不暴露到头文件)
        // ═══════════════════════════════════════════════════════════════

        // 自动查找tessdata路径
        // 只做路径搜索, 不设置环境变量, 不附带任何默认搜索路径
        // 使用 fs::path 处理宽字符/Unicode路径
        std::string _findTessdata(const std::string& hint)
        {
            namespace fs = std::filesystem;

            // 1. 手动指定路径
            if (!hint.empty()) {
                if (auxiliaryTools::_hasTraineddata(fs::path(hint)))
                    return hint;
            }

            // 2. ./exe 目录下
            if (auxiliaryTools::_hasTraineddata("./exe/tessdata"))
                return "./exe/tessdata";
            if (auxiliaryTools::_hasTraineddata("./exe"))
                return "./exe";

            return "";
        }

        // 预处理图像为32位深度 (提升OCR精度)
        // 通过 fs::path + pixReadMem 支持Unicode/中文路径
        Pix* _preprocessImage(const std::string& imgPath) {
            namespace fs = std::filesystem;

            // 使用 fs::path 处理宽字符路径, 读入内存后用 pixReadMem 解码
            fs::path fsp(imgPath);

#ifdef _WIN32
            // Windows: _wfopen 支持Unicode路径
            FILE* fp = _wfopen(fsp.wstring().c_str(), L"rb");
            if (!fp) return nullptr;

            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            std::vector<unsigned char> buf(fsize);
            fread(buf.data(), 1, fsize, fp);
            fclose(fp);

            Pix* pix = pixReadMem(buf.data(), buf.size());
#else
            Pix* pix = pixRead(imgPath.c_str());
#endif
            if (!pix) return nullptr;

            l_int32 d = pixGetDepth(pix);
            Pix* pix32 = nullptr;

            if (d == 1)
                pix32 = pixConvert8To32(pixConvert1To8(nullptr, pix, 255, 0));
            else if (d == 8)
                pix32 = pixConvert8To32(pix);
            else if (d == 32)
                pix32 = pixClone(pix);
            else
                pix32 = pixConvertTo32(pix);

            pixDestroy(&pix);
            return pix32;
        }

    } // namespace (anonymous)

// ═══════════════════════════════════════════════════════════════
// 构造 / 析构
// ═══════════════════════════════════════════════════════════════

    Ocr::Ocr(const std::string& lang, const std::string& tessdataDir) {
        tessdata_ = tessdataDir.empty() ? _findTessdata("") : _findTessdata(tessdataDir);
        if (tessdata_.empty()) {
            fprintf(stderr, "[OCR:FATAL] tessdata not found!\n"
                            "  Set TESSDATA_PREFIX or pass tessdataDir to constructor.\n");
            return;
        }

        api_ = new tesseract::TessBaseAPI();
        // 使用 fs::path 确保宽字符路径正确传递给Tesseract
        int rc = api_->Init(std::filesystem::path(tessdata_).string().c_str(),
                            lang.c_str(), tesseract::OEM_LSTM_ONLY);
        if (rc != 0) {
            fprintf(stderr, "[OCR:ERROR] Tesseract init failed (rc=%d)\n", rc);
            fprintf(stderr, "  lang:           %s\n", lang.c_str());
            fprintf(stderr, "  tessdata prefix: %s\n", tessdata_.c_str());
            delete api_;
            api_ = nullptr;
            return;
        }

        api_->SetPageSegMode(tesseract::PSM_AUTO);
        api_->SetVariable("preserve_interword_spaces", "1");
        ok_ = true;
    }

    Ocr::~Ocr() {
        if (api_) {
            api_->End();
            delete api_;
        }
    }

// ═══════════════════════════════════════════════════════════════
// 属性查询
// ═══════════════════════════════════════════════════════════════

    bool Ocr::ready() const { return ok_ && api_ != nullptr; }

    const std::string& Ocr::tessdataPath() const { return tessdata_; }

// ═══════════════════════════════════════════════════════════════
// 切换语言
// ═══════════════════════════════════════════════════════════════

    bool Ocr::setLanguage(const std::string& lang) {
        if (api_) {
            api_->End();
            delete api_;
            api_ = nullptr;
            ok_ = false;
        }

        api_ = new tesseract::TessBaseAPI();
        // 使用 fs::path 确保宽字符路径正确传递给Tesseract
        int rc = api_->Init(std::filesystem::path(tessdata_).string().c_str(),
                            lang.c_str(), tesseract::OEM_LSTM_ONLY);
        if (rc != 0) {
            fprintf(stderr, "[OCR:ERROR] Language switch failed (rc=%d) for '%s'\n", rc, lang.c_str());
            delete api_;
            api_ = nullptr;
            return false;
        }

        api_->SetPageSegMode(tesseract::PSM_AUTO);
        api_->SetVariable("preserve_interword_spaces", "1");
        ok_ = true;
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 全文识别
    // ═══════════════════════════════════════════════════════════════

    std::string Ocr::readImageText(const std::string& imgPath, int* outConf) {
        if (!ready()) return {};

        Pix* pix = _preprocessImage(imgPath);
        if (!pix) {
            fprintf(stderr, "[OCR:WARN] Cannot open image: %s\n", imgPath.c_str());
            return {};
        }

        api_->SetImage(pix);
        char* out = api_->GetUTF8Text();
        std::string result(out ? out : "");
        delete[] out;

        if (outConf) *outConf = api_->MeanTextConf();

        api_->Clear();
        pixDestroy(&pix);
        return result;
    }

// ═══════════════════════════════════════════════════════════════
// 逐行识别
// ═══════════════════════════════════════════════════════════════

    std::vector<std::string> Ocr::readImageTextLine(const std::string& imgPath,
                                                    int* outConf) {
        if (!ready()) return {};

        Pix* pix = _preprocessImage(imgPath);
        if (!pix) {
            fprintf(stderr, "[OCR:WARN] Cannot open image: %s\n", imgPath.c_str());
            return {};
        }

        api_->SetImage(pix);
        api_->Recognize(nullptr);  // 必须先Recognize才能用ResultIterator

        if (outConf) *outConf = api_->MeanTextConf();

        std::vector<std::string> lines;
        tesseract::ResultIterator* ri = api_->GetIterator();

        if (ri && !ri->Empty(tesseract::RIL_TEXTLINE)) {
            do {
                char* line = ri->GetUTF8Text(tesseract::RIL_TEXTLINE);
                if (line) {
                    std::string s(line);
                    delete[] line;
                    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                        s.pop_back();
                    if (!s.empty()) lines.push_back(std::move(s));
                }
            } while (ri->Next(tesseract::RIL_TEXTLINE));
            delete ri;
        } else {
            // 备选方案: 无布局信息时按换行符分割全文
            delete ri;
            char* out = api_->GetUTF8Text();
            if (out) {
                std::string full(out);
                delete[] out;
                size_t start = 0;
                for (size_t i = 0; i <= full.size(); ++i) {
                    if (i == full.size() || full[i] == '\n') {
                        std::string line = full.substr(start, i - start);
                        while (!line.empty() && line.back() == '\r')
                            line.pop_back();
                        if (!line.empty()) lines.push_back(std::move(line));
                        start = i + 1;
                    }
                }
            }
        }

        api_->Clear();
        pixDestroy(&pix);
        return lines;
    }

} // namespace ocr