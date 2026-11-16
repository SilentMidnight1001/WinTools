#pragma once
/**
 * wtlOcr.hpp - RapidOCR(onnxruntime) OCR module.
 *
 * The public interface is independent from winToolsH.hpp. RapidOCR's C API is
 * used only by wtlOcr.cpp and is not exposed to library users.
 */

#include <string>
#include <vector>

#include <wtlMediaToolkit.hpp>

namespace ocr
{
    /**
     * Ocr - RapidOCR(onnxruntime) wrapper.
     *
     * Usage:
     *   ocr::Ocr engine;                       // find the models directory automatically
     *   ocr::Ocr engine("D:/radioOCR/models"); // specify the models directory manually
     *   if (engine.ready()) {
     *       std::string text = engine.readImageText("img.png");
     *       auto lines = engine.readImageTextLine("img.png");
     *   }
     */
    class Ocr
    {
    public:
        // modelsDir contains the det/cls/rec onnx files and ppocrv6_dict.txt.
        // Empty means auto-detect the runtime model directory and the compile-time
        // RADIOOCR_MODELS_DIR path.
        explicit Ocr(const std::string& modelsDir = "");
        ~Ocr();

        // Whether the engine initialized successfully.
        bool ready() const { return ok_ && handle_ != nullptr; }

        // The models directory currently in use.
        const std::string& modelsPath() const;

        // Recognize all text in an image (.png/.jpg/.bmp/.tif/.webp).
        // outConf optionally receives the average confidence (0-100).
        std::string readImageText(const std::string& imgPath,
                                  int* outConf = nullptr);

        // Recognize image text line by line.
        // outConf optionally receives the average confidence (0-100).
        std::vector<std::string> readImageTextLine(const std::string& imgPath,
                                                   int* outConf = nullptr);

        // Find a button by text in the current virtual screen and return its rectangle.
        // Matching ignores whitespace and ASCII case. Returns all -1 and result=false
        // when the text is not found.
        wtl::ImageRect getButtonRect(const std::string& buttonName);

        // Find a button by text in the current virtual screen and return its center.
        // Returns -1 coordinates and result=false when the text is not found.
        wtl::ImagePosition getButtonPosition(const std::string& buttonName);

        // Return all matching text rectangles in the current virtual screen, ordered by
        // match priority and then top-to-bottom, left-to-right.
        std::vector<wtl::ImageRect> getButtonRectVec(const std::string& buttonName);

        // Return all matching text centers in the same order as getButtonRectVec().
        std::vector<wtl::ImagePosition> getButtonPositionVec(const std::string& buttonName);

    private:
        // RapidOCR OCR_HANDLE. Stored as void* to keep OcrLiteCApi.h out of this header.
        void* handle_ = nullptr;
        std::string modelsDir_;
        bool ok_ = false;
    };

} // namespace ocr
