#include <wtlOcr.hpp>
// RapidOCR (OcrLite C API) — 仅cpp内部使用, 不暴露给库的使用者
#include <OcrLiteCApi.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace ocr {
    namespace {

        // ═══════════════════════════════════════════════════════════════
        // 内部辅助函数 (自由函数, 不暴露到头文件)
        // ═══════════════════════════════════════════════════════════════

        namespace fs = std::filesystem;

        // 与 <工程根>/model 下打包的模型文件同名
        // PP-OCRv6(small) 比 v3 mobile 强不少, 尤其对符号/大小写; 字典随模型元数据导出
        constexpr const char* _kDetModel = "PP-OCRv6_det_small.onnx";
        constexpr const char* _kClsModel = "ch_ppocr_mobile_v2.0_cls_mobile.onnx";
        constexpr const char* _kRecModel = "PP-OCRv6_rec_small.onnx";
        constexpr const char* _kKeysFile = "ppocrv6_dict.txt";

        // 与参考工程 radioocr 一致, 4 线程
        constexpr int _kNumThreads = 4;

        // 路径是否为纯 ASCII (RapidOCR 内部逐字节加宽窄字符串, 非 ASCII 会乱码)
        bool _isAscii(const std::string& s) {
            for (unsigned char c : s) {
                if (c >= 0x80) return false;
            }
            return true;
        }

        // 中文路径退一步: 取 Windows 8.3 短名, 得到一条等价的纯 ASCII 路径 //
        // (卷上关闭了 8.3 名称时 GetShortPathNameW 返回原路径, 由调用方再判一次 ASCII) //
        fs::path _asciiShortPath(const fs::path& p) {
            const std::wstring w = p.wstring();
            if (w.empty()) return fs::path();

            const DWORD need = GetShortPathNameW(w.c_str(), nullptr, 0);
            if (need == 0) return fs::path();

            std::wstring buf(need, L'\0');
            const DWORD len = GetShortPathNameW(w.c_str(), buf.data(), need);
            if (len == 0 || len >= need) return fs::path();

            buf.resize(len);
            return fs::path(buf);
        }

        // 交给 RapidOCR 的路径必须是纯 ASCII (它逐字节加宽窄字符串):
        // 已经是 ASCII 就直接用; 否则依次尝试"相对当前目录"和 Windows 8.3 短名, 都拿不到返回空
        fs::path _asciiPathFor(const fs::path& p) {
            if (_isAscii(p.u8string())) return p;

            // 相对路径由系统按当前目录解析, 路径串本身是 ASCII 就够了
            std::error_code ec;
            const fs::path rel = fs::relative(p, fs::current_path(ec), ec);
            if (!ec && !rel.empty() && _isAscii(rel.u8string()))
                return rel;

            const fs::path shortPath = _asciiShortPath(p);
            if (!shortPath.empty() && _isAscii(shortPath.u8string()))
                return shortPath;

            return fs::path();
        }

        // 检查目录中是否备齐四个模型文件
        // 只做路径检查, 不设置环境变量, 不附带任何默认搜索路径
        bool _hasModelFiles(const fs::path& dir) {
            std::error_code ec;
            if (!fs::is_directory(dir, ec)) return false;
            return fs::is_regular_file(dir / _kDetModel, ec)
                   && fs::is_regular_file(dir / _kClsModel, ec)
                   && fs::is_regular_file(dir / _kRecModel, ec)
                   && fs::is_regular_file(dir / _kKeysFile, ec);
        }

        // 自动查找模型目录: 手动指定 -> 运行目录 ./model -> 编译期 RADIOOCR_MODELS_DIR
        // 字符串按 UTF-8 解析(fs::u8path), 中文路径不乱码, 结果同样以 UTF-8 存回 std::string
        std::string _findModelsDir(const std::string& hint) {
            if (!hint.empty()) {
                const fs::path hintDir = fs::u8path(hint);
                if (_hasModelFiles(hintDir))
                    return hintDir.u8string();
            }

            const fs::path localDir = fs::u8path("./model");
            if (_hasModelFiles(localDir))
                return localDir.u8string();

#ifdef RADIOOCR_MODELS_DIR
            const fs::path builtinDir = fs::u8path(RADIOOCR_MODELS_DIR);
            if (_hasModelFiles(builtinDir))
                return builtinDir.u8string();
#endif

            return "";
        }

        // 图片读入内存: 走宽字符路径, 中文路径不乱码; 同时不把路径交给库
        // (库内部按本地编码打开文件, 中文路径会踩坑, 所以只喂它图片字节)
        bool _readFileBytes(const fs::path& path, std::string& bytes) {
            FILE* fp = _wfopen(path.wstring().c_str(), L"rb");
            if (!fp) return false;

            fseek(fp, 0, SEEK_END);
            const long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if (size <= 0) {
                fclose(fp);
                return false;
            }

            bytes.resize(static_cast<size_t>(size));
            const size_t read = fread(&bytes[0], 1, bytes.size(), fp);
            fclose(fp);

            if (read != bytes.size()) {
                bytes.clear();
                return false;
            }
            return true;
        }

        // 与参考工程 radioocr/demo.cpp 一致的检测参数
        OCR_PARAM _defaultParam() {
            OCR_PARAM param = {};
            param.padding = 50;
            param.maxSideLen = 1024;
            param.boxScoreThresh = 0.5f;
            param.boxThresh = 0.3f;
            param.unClipRatio = 1.6f;
            param.doAngle = 1;
            param.mostAngle = 1;
            return param;
        }

        // 极性归一化: 模型按"暗字亮底"训练, 而深色主题截图是"亮字暗底", 不处理会明显掉字
        // (分号读成冒号、闭合引号读成全角括号、大小写乱)。用 Otsu 统计暗像素占比判断底色;
        // 顺便转灰度, 消掉语法高亮的彩色干扰。
        cv::Mat _normalizePolarity(const cv::Mat& src) {
            cv::Mat gray;
            cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

            cv::Mat mask;
            cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
            const double darkRatio = 1.0 - cv::countNonZero(mask) / static_cast<double>(mask.total());

            cv::Mat out;
            if (darkRatio > 0.6) {
                cv::bitwise_not(gray, out);   // 暗底亮字 -> 亮底暗字
            } else {
                out = gray;
            }

            cv::cvtColor(out, out, cv::COLOR_GRAY2BGR);   // 三通道等值, 交给位图接口
            return out;
        }

        // 整张图片识别一次, 结果写进 result (调用方负责 OcrFreeResult)
        bool _detectImage(OCR_HANDLE handle, const std::string& imgPath, OCR_RESULT& result) {
            std::string bytes;
            if (!_readFileBytes(fs::u8path(imgPath), bytes)) {
                fprintf(stderr, "[OCR:WARN] Cannot open image: %s\n", imgPath.c_str());
                return false;
            }

            const cv::Mat raw(1, static_cast<int>(bytes.size()), CV_8UC1,
                              const_cast<char*>(bytes.data()));
            const cv::Mat src = cv::imdecode(raw, cv::IMREAD_COLOR);
            if (src.empty()) {
                fprintf(stderr, "[OCR:WARN] Cannot decode image: %s\n", imgPath.c_str());
                return false;
            }

            // 自己解码: 先做极性归一化(深色主题截图是亮字暗底), 再走库的位图接口
            const cv::Mat prepared = _normalizePolarity(src);

            OCR_PARAM param = _defaultParam();

            OCR_INPUT input = {};
            input.data = const_cast<uint8_t*>(prepared.data);
            input.type = 0;  // 0 = 位图 (三通道等值, RGB/BGR 无差别)
            input.channels = 3;
            input.width = prepared.cols;
            input.height = prepared.rows;
            input.dataLength = static_cast<long>(prepared.total() * prepared.elemSize());

            try {
                return OcrDetectInput(handle, &input, &param, &result) != FALSE;
            } catch (const std::exception& e) {
                // 图片解码失败时库内会抛异常
                fprintf(stderr, "[OCR:WARN] Detect failed: %s\n", e.what());
                return false;
            }
        }

        // 库返回的 TEXT_BLOCK.text 不保证带结束符, 按 textLength 取
        std::string _blockText(const TEXT_BLOCK& block) {
            const size_t len = block.textLength > 0 ? static_cast<size_t>(block.textLength) - 1 : 0;
            return std::string(reinterpret_cast<const char*>(block.text), len);
        }

        // UTF-8 字符个数 (续字节不计数)
        size_t _charCount(const std::string& utf8) {
            size_t count = 0;
            for (unsigned char c : utf8) {
                if ((c & 0xC0) != 0x80) ++count;
            }
            return count;
        }

        // 检测框的左/右边界 (4 个角点取极值)
        int _leftEdge(const TEXT_BLOCK& block) {
            if (block.boxPointLength == 0) return 0;
            int x = static_cast<int>(block.boxPoint[0].x);
            for (unsigned long long i = 1; i < block.boxPointLength; ++i) {
                x = (std::min)(x, static_cast<int>(block.boxPoint[i].x));
            }
            return x;
        }

        int _rightEdge(const TEXT_BLOCK& block) {
            if (block.boxPointLength == 0) return 0;
            int x = static_cast<int>(block.boxPoint[0].x);
            for (unsigned long long i = 1; i < block.boxPointLength; ++i) {
                x = (std::max)(x, static_cast<int>(block.boxPoint[i].x));
            }
            return x;
        }

        // 检测框的上边界 (4 个角点取最小值), 用来排阅读顺序
        int _topEdge(const TEXT_BLOCK& block) {
            if (block.boxPointLength == 0) return 0;
            int y = static_cast<int>(block.boxPoint[0].y);
            for (unsigned long long i = 1; i < block.boxPointLength; ++i) {
                y = (std::min)(y, static_cast<int>(block.boxPoint[i].y));
            }
            return y;
        }

        // 库返回的是检测顺序, 不一定是阅读顺序(截图里行号、图标会和正文交错),
        // 这里按 上→下、左→右 重排一遍
        std::vector<size_t> _readingOrder(const OCR_RESULT& result) {
            std::vector<size_t> order(static_cast<size_t>(result.textBlocksLength));
            for (size_t i = 0; i < order.size(); ++i) order[i] = i;

            std::sort(order.begin(), order.end(), [&result](size_t a, size_t b) {
                const int ta = _topEdge(result.textBlocks[a]);
                const int tb = _topEdge(result.textBlocks[b]);
                if (ta != tb) return ta < tb;
                return _leftEdge(result.textBlocks[a]) < _leftEdge(result.textBlocks[b]);
            });
            return order;
        }

        // 平均置信度(0-100): 优先用各块的字符识别置信度, 没有字符分数时退回检测框分数
        int _meanConfidence(const OCR_RESULT& result) {
            double sum = 0.0;
            size_t count = 0;
            for (unsigned long long i = 0; i < result.textBlocksLength; ++i) {
                const TEXT_BLOCK& block = result.textBlocks[i];
                for (unsigned long long j = 0; j < block.charScoresLength; ++j) {
                    sum += block.charScores[j];
                    ++count;
                }
            }
            if (count > 0)
                return static_cast<int>(std::lround(sum / double(count) * 100.0));

            double boxSum = 0.0;
            for (unsigned long long i = 0; i < result.textBlocksLength; ++i) {
                boxSum += result.textBlocks[i].boxScore;
            }
            if (result.textBlocksLength == 0) return 0;
            return static_cast<int>(std::lround(boxSum / double(result.textBlocksLength) * 100.0));
        }

        // 把检测结果按行整理
        // 行首缩进在检测阶段就丢了(框只圈文字), 这里按左边界 x 把空格补回来
        // 低检测分且极短的块多半是图标/装饰(截图里常见), 丢掉避免混进结果
        constexpr float _kMinBoxScore = 0.55f;
        constexpr size_t _kShortBlockChars = 2;

        bool _isNoiseBlock(const TEXT_BLOCK& block, const std::string& text) {
            if (block.boxScore >= _kMinBoxScore) return false;
            return _charCount(text) <= _kShortBlockChars;
        }

        std::vector<std::string> _toLines(const OCR_RESULT& result) {
            std::vector<std::string> lines;
            if (result.textBlocksLength == 0) return lines;

            const std::vector<size_t> order = _readingOrder(result);

            // 字符平均宽度: 各块(宽度/字符数)的中位数, 用来把左边距换算成行首空格
            std::vector<double> charWidths;
            int minLeft = _leftEdge(result.textBlocks[0]);
            for (size_t idx : order) {
                const TEXT_BLOCK& block = result.textBlocks[idx];
                if (_isNoiseBlock(block, _blockText(block))) continue;

                minLeft = (std::min)(minLeft, _leftEdge(block));

                const size_t chars = _charCount(_blockText(block));
                if (chars > 0)
                    charWidths.push_back(double(_rightEdge(block) - _leftEdge(block)) / double(chars));
            }
            std::sort(charWidths.begin(), charWidths.end());
            const double charWidth = charWidths.empty() ? 0.0 : charWidths[charWidths.size() / 2];

            lines.reserve(order.size());
            for (size_t idx : order) {
                const TEXT_BLOCK& block = result.textBlocks[idx];
                const std::string blockTxt = _blockText(block);
                if (_isNoiseBlock(block, blockTxt)) continue;

                std::string line;
                if (charWidth > 0.0) {
                    const double indent = std::lround((_leftEdge(block) - minLeft) / charWidth);
                    line.append(static_cast<size_t>((std::max)(indent, 0.0)), ' ');
                }
                line.append(blockTxt);
                lines.push_back(std::move(line));
            }
            return lines;
        }

        struct _ScreenImage {
            int originX = 0;
            int originY = 0;
            int width = 0;
            int height = 0;
            cv::Mat bgr;
        };

        // 保证截图过程中任何提前返回或 OpenCV 异常都不会泄漏 GDI 对象
        class _GdiScreenResources {
        public:
            HDC screen = nullptr;
            HDC memory = nullptr;
            HBITMAP bitmap = nullptr;
            HGDIOBJ previous = nullptr;

            ~_GdiScreenResources() {
                if (memory && previous) SelectObject(memory, previous);
                if (bitmap) DeleteObject(bitmap);
                if (memory) DeleteDC(memory);
                if (screen) ReleaseDC(nullptr, screen);
            }
        };

        bool _captureVirtualScreen(_ScreenImage& image) {
            image = _ScreenImage{};
            SetProcessDPIAware();

            const int originX = GetSystemMetrics(SM_XVIRTUALSCREEN);
            const int originY = GetSystemMetrics(SM_YVIRTUALSCREEN);
            const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            if (width <= 0 || height <= 0) return false;

            _GdiScreenResources gdi;
            gdi.screen = GetDC(nullptr);
            if (!gdi.screen) return false;

            gdi.memory = CreateCompatibleDC(gdi.screen);
            if (!gdi.memory) return false;

            BITMAPINFO bitmapInfo = {};
            bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmapInfo.bmiHeader.biWidth = width;
            bitmapInfo.bmiHeader.biHeight = -height; // 负数: 从上到下存储
            bitmapInfo.bmiHeader.biPlanes = 1;
            bitmapInfo.bmiHeader.biBitCount = 32;
            bitmapInfo.bmiHeader.biCompression = BI_RGB;

            void* bgraData = nullptr;
            gdi.bitmap = CreateDIBSection(gdi.screen, &bitmapInfo, DIB_RGB_COLORS,
                                          &bgraData, nullptr, 0);
            if (!gdi.bitmap || !bgraData) return false;

            HGDIOBJ previous = SelectObject(gdi.memory, gdi.bitmap);
            if (!previous || previous == HGDI_ERROR) return false;
            gdi.previous = previous;

            if (!BitBlt(gdi.memory, 0, 0, width, height,
                        gdi.screen, originX, originY, SRCCOPY)) {
                return false;
            }

            const cv::Mat bgraView(height, width, CV_8UC4, bgraData);
            cv::cvtColor(bgraView, image.bgr, cv::COLOR_BGRA2BGR);

            image.originX = originX;
            image.originY = originY;
            image.width = width;
            image.height = height;
            return true;
        }

        bool _detectScreenImage(OCR_HANDLE handle, const _ScreenImage& image,
                                OCR_RESULT& result) {
            if (!handle || image.bgr.empty()) return false;

            const cv::Mat prepared = _normalizePolarity(image.bgr);
            const size_t byteLength = prepared.total() * prepared.elemSize();
            if (byteLength > static_cast<size_t>((std::numeric_limits<long>::max)()))
                return false;

            OCR_PARAM param = _defaultParam();
            param.padding = 20;
            param.maxSideLen = (std::max)(image.width, image.height);

            OCR_INPUT input = {};
            input.data = const_cast<uint8_t*>(prepared.data);
            input.type = 0;
            input.channels = 3;
            input.width = prepared.cols;
            input.height = prepared.rows;
            input.dataLength = static_cast<long>(byteLength);

            try {
                return OcrDetectInput(handle, &input, &param, &result) != FALSE;
            } catch (const std::exception& e) {
                fprintf(stderr, "[OCR:WARN] Screen detect failed: %s\n", e.what());
                return false;
            }
        }

        bool _isAsciiSpace(unsigned char c) {
            return c == ' ' || c == '\t' || c == '\n' ||
                   c == '\r' || c == '\f' || c == '\v';
        }

        // 去掉 OCR 常见空白并统一 ASCII 大小写，避免 "登 录" / "Login" 无法匹配
        std::string _normalizeButtonName(const std::string& text) {
            std::string normalized;
            normalized.reserve(text.size());

            for (size_t i = 0; i < text.size();) {
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c < 0x80) {
                    if (!_isAsciiSpace(c)) {
                        normalized.push_back(c >= 'A' && c <= 'Z'
                                                 ? static_cast<char>(c - 'A' + 'a')
                                                 : static_cast<char>(c));
                    }
                    ++i;
                    continue;
                }

                // U+00A0 / U+3000 / U+200B: 不换行空格、全角空格、零宽空格
                if (i + 1 < text.size() && c == 0xC2 &&
                    static_cast<unsigned char>(text[i + 1]) == 0xA0) {
                    i += 2;
                    continue;
                }
                if (i + 2 < text.size() && c == 0xE3 &&
                    static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                    static_cast<unsigned char>(text[i + 2]) == 0x80) {
                    i += 3;
                    continue;
                }
                if (i + 2 < text.size() && c == 0xE2 &&
                    static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                    static_cast<unsigned char>(text[i + 2]) == 0x8B) {
                    i += 3;
                    continue;
                }

                normalized.push_back(text[i]);
                ++i;
            }
            return normalized;
        }

        // 精确匹配优先，其次允许 OCR 在一个文字块中带入额外标点或说明
        int _matchRank(const std::string& ocrText, const std::string& wantedText) {
            const std::string ocr = _normalizeButtonName(ocrText);
            const std::string wanted = _normalizeButtonName(wantedText);
            if (ocr.empty() || wanted.empty()) return 0;
            if (ocr == wanted) return 3;
            if (ocr.find(wanted) != std::string::npos) return 2;
            if (_charCount(ocr) >= 2 && wanted.find(ocr) != std::string::npos) return 1;
            return 0;
        }

        int _bottomEdge(const TEXT_BLOCK& block) {
            if (block.boxPointLength == 0) return 0;
            int y = static_cast<int>(block.boxPoint[0].y);
            for (unsigned long long i = 1; i < block.boxPointLength; ++i) {
                y = (std::max)(y, static_cast<int>(block.boxPoint[i].y));
            }
            return y;
        }

        double _textConfidence(const TEXT_BLOCK& block) {
            if (block.charScoresLength == 0) return block.boxScore;

            double sum = 0.0;
            for (unsigned long long i = 0; i < block.charScoresLength; ++i) {
                sum += block.charScores[i];
            }
            return sum / static_cast<double>(block.charScoresLength);
        }

        int _clampPixel(int value, int size) {
            return (std::max)(0, (std::min)(value, size - 1));
        }

        wtl::ImageRect _blockScreenRect(const TEXT_BLOCK& block,
                                        const _ScreenImage& image) {
            wtl::ImageRect rect{-1, -1, -1, -1, false};
            if (block.boxPointLength == 0) return rect;

            const int left = _clampPixel(_leftEdge(block), image.width);
            const int top = _clampPixel(_topEdge(block), image.height);
            const int right = _clampPixel(_rightEdge(block), image.width) + 1;
            const int bottom = _clampPixel(_bottomEdge(block), image.height) + 1;

            rect.topX = image.originX + left;
            rect.topY = image.originY + top;
            rect.btnX = image.originX + right;
            rect.btnY = image.originY + bottom;
            rect.result = true;
            return rect;
        }

        class _OcrResultGuard {
        public:
            OCR_RESULT result = {};

            ~_OcrResultGuard() {
                OcrFreeResult(&result);
            }

            _OcrResultGuard() = default;
            _OcrResultGuard(const _OcrResultGuard&) = delete;
            _OcrResultGuard& operator=(const _OcrResultGuard&) = delete;
        };

    } // namespace (anonymous)

// ═══════════════════════════════════════════════════════════════
// 构造 / 析构 (声明对象即初始化, 析构自动释放)
// ═══════════════════════════════════════════════════════════════

    Ocr::Ocr(const std::string& modelsDir) {
        modelsDir_ = modelsDir.empty() ? _findModelsDir("") : _findModelsDir(modelsDir);
        if (modelsDir_.empty()) {
            fprintf(stderr, "[OCR:FATAL] models dir not found!\n"
                            "  Pass modelsDir to the constructor, or put model/ next to the exe.\n");
            return;
        }

        // RapidOCR 内部把窄字符路径逐字节转宽字符(OcrUtils::strToWstr), 非 ASCII 路径会乱码:
        // 路径先按 UTF-8 解析, 中文路径再换成"相对当前目录/8.3 短名"等等价 ASCII 形式交给它
        fs::path dir = fs::u8path(modelsDir_);
        if (!_isAscii(dir.u8string())) {
            const fs::path asciiDir = _asciiPathFor(dir);
            if (asciiDir.empty()) {
                // RapidOCR 内部逐字节加宽窄字符串, 非 ASCII 路径它必然加载失败;
                // MSVC 版失败时还会直接终止进程(fail-fast), 所以这里不再往下调用
                fprintf(stderr, "[OCR:FATAL] models path is not ASCII and has no ASCII form:\n"
                                "  %s\n"
                                "  Put model/ next to the exe, or use an ASCII path.\n",
                        modelsDir_.c_str());
                return;
            }
            dir = asciiDir;
        }

        const std::string detPath = (dir / _kDetModel).string();
        const std::string clsPath = (dir / _kClsModel).string();
        const std::string recPath = (dir / _kRecModel).string();
        const std::string keysPath = (dir / _kKeysFile).string();

        try {
            handle_ = OcrInit(detPath.c_str(), clsPath.c_str(), recPath.c_str(),
                              keysPath.c_str(), _kNumThreads);
        } catch (const std::exception& e) {
            // 模型缺失/损坏时, onnxruntime 加载模型会抛异常
            fprintf(stderr, "[OCR:ERROR] RapidOCR init failed: %s\n", e.what());
            handle_ = nullptr;
            return;
        }

        if (!handle_) {
            fprintf(stderr, "[OCR:ERROR] RapidOCR init failed (handle is null)\n"
                            "  models dir: %s\n", modelsDir_.c_str());
            return;
        }

        ok_ = true;
    }

    Ocr::~Ocr() {
        if (handle_) {
            OcrDestroy(handle_);
            handle_ = nullptr;
        }
        ok_ = false;
    }

// ═══════════════════════════════════════════════════════════════
// 属性查询 (ready() 直接内联在头文件的类里, 这里只剩 modelsPath())
// ═══════════════════════════════════════════════════════════════

    const std::string& Ocr::modelsPath() const { return modelsDir_; }

// ═══════════════════════════════════════════════════════════════
// 文字识别
// ═══════════════════════════════════════════════════════════════

    std::vector<std::string> Ocr::readImageTextLine(const std::string& imgPath, int* outConf) {
        if (!ready()) return {};

        OCR_RESULT result = {};
        if (!_detectImage(handle_, imgPath, result)) return {};
        if (result.textBlocksLength == 0) {
            OcrFreeResult(&result);
            return {};
        }

        if (outConf) *outConf = _meanConfidence(result);

        std::vector<std::string> lines = _toLines(result);
        OcrFreeResult(&result);
        return lines;
    }

    std::string Ocr::readImageText(const std::string& imgPath, int* outConf) {
        // 逐行结果拼回整段文本, 保证 join(readImageTextLine(), "\n") == readImageText()
        const std::vector<std::string> lines = readImageTextLine(imgPath, outConf);

        std::string text;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) text.push_back('\n');
            text.append(lines[i]);
        }
        return text;
    }

    std::vector<wtl::ImageRect> Ocr::getButtonRectVec(const std::string& buttonName) {
        std::vector<wtl::ImageRect> found;
        if (!ready() || _normalizeButtonName(buttonName).empty()) return found;

        _ScreenImage image;
        try {
            if (!_captureVirtualScreen(image)) {
                fprintf(stderr, "[OCR:WARN] Cannot capture virtual screen.\n");
                return found;
            }
        } catch (const std::exception& e) {
            fprintf(stderr, "[OCR:WARN] Cannot capture virtual screen: %s\n", e.what());
            return found;
        }

        _OcrResultGuard held;
        if (!_detectScreenImage(handle_, image, held.result) ||
            held.result.textBlocksLength == 0) {
            return found;
        }

        struct Match {
            int rank;
            int top;
            int left;
            double confidence;
            wtl::ImageRect rect;
        };
        std::vector<Match> matches;

        for (unsigned long long i = 0; i < held.result.textBlocksLength; ++i) {
            const TEXT_BLOCK& block = held.result.textBlocks[i];
            if (block.boxPointLength == 0) continue;

            const int rank = _matchRank(_blockText(block), buttonName);
            if (rank == 0) continue;

            matches.push_back({rank, _topEdge(block), _leftEdge(block),
                               _textConfidence(block), _blockScreenRect(block, image)});
        }

        std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
            if (a.rank != b.rank) return a.rank > b.rank;
            if (a.top != b.top) return a.top < b.top;
            if (a.left != b.left) return a.left < b.left;
            return a.confidence > b.confidence;
        });

        // OCR 框会有 1-2px 的高度抖动；按可视行分组后再从左到右排序，
        // 避免同一排按钮在两次识别中的顺序发生变化。
        const auto sameVisualRow = [](const Match& a, const Match& b) {
            const int aHeight = a.rect.btnY - a.rect.topY;
            const int bHeight = b.rect.btnY - b.rect.topY;
            const int tolerance = (std::max)(8, (std::max)(aHeight, bHeight) / 2);
            const int aCenter = a.rect.topY + aHeight / 2;
            const int bCenter = b.rect.topY + bHeight / 2;
            return std::abs(aCenter - bCenter) <= tolerance;
        };

        for (size_t rowStart = 0; rowStart < matches.size();) {
            size_t rowEnd = rowStart + 1;
            while (rowEnd < matches.size() &&
                   matches[rowEnd].rank == matches[rowStart].rank &&
                   sameVisualRow(matches[rowStart], matches[rowEnd])) {
                ++rowEnd;
            }

            std::sort(matches.begin() + static_cast<std::ptrdiff_t>(rowStart),
                      matches.begin() + static_cast<std::ptrdiff_t>(rowEnd),
                      [](const Match& a, const Match& b) {
                          if (a.left != b.left) return a.left < b.left;
                          if (a.top != b.top) return a.top < b.top;
                          return a.confidence > b.confidence;
                      });
            rowStart = rowEnd;
        }

        found.reserve(matches.size());
        for (const Match& match : matches) found.push_back(match.rect);
        return found;
    }

    wtl::ImageRect Ocr::getButtonRect(const std::string& buttonName) {
        const std::vector<wtl::ImageRect> found = getButtonRectVec(buttonName);
        if (found.empty()) return {-1, -1, -1, -1, false};
        return found.front();
    }

    std::vector<wtl::ImagePosition> Ocr::getButtonPositionVec(
            const std::string& buttonName) {
        const std::vector<wtl::ImageRect> rects = getButtonRectVec(buttonName);
        std::vector<wtl::ImagePosition> positions;
        positions.reserve(rects.size());

        for (const wtl::ImageRect& rect : rects) {
            positions.push_back({rect.topX + (rect.btnX - rect.topX) / 2,
                                 rect.topY + (rect.btnY - rect.topY) / 2,
                                 true});
        }
        return positions;
    }

    wtl::ImagePosition Ocr::getButtonPosition(const std::string& buttonName) {
        const std::vector<wtl::ImagePosition> found = getButtonPositionVec(buttonName);
        if (found.empty()) return {-1, -1, false};
        return found.front();
    }

} // namespace ocr
