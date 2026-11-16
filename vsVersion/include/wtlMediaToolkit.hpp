#pragma once
/**
 * wtlMediaToolkit.hpp - 图像、视频与音频工具库（C++17 / Windows）
 *
 * ImageTools、VideoTools、AudioTools 的公开接口统一位于 namespace wtl。
 * 非模板实现位于 wtlMediaToolkit.cpp，可单独构建为 wtlMediaToolkit。
 * Public ImageTools, VideoTools, and AudioTools interfaces live in namespace wtl.
 * Non-template implementations are in wtlMediaToolkit.cpp and can build as wtlMediaToolkit.
 */

#include <windows.h>
#include <memory>
#include <string>
#include <vector>

namespace wtl
{
    /***************** 图像处理类 ******************/

    // 图像坐标与匹配结果
    // Image coordinates and match result.
    struct ImagePosition
    {
        int x;
        int y;
        bool result;
    };

    // 模板在主图中的矩形位置
    // Template rectangle within the source image.
    struct ImageRect
    {
        int topX;
        int topY;
        int btnX;
        int btnY;
        bool result;
    };

    // 图像尺寸与通道数
    // Image dimensions and channel count.
    struct ImageSize
    {
        int x;
        int y;
        int channels;
    };

    /**
     * 图像识别、截图与图像处理类。
     *
     * 功能包括模板匹配、区域识别、相似图片比较、截图、缩放、格式转换、
     * 水印移除和矩形绘制。
     * Provides template matching, region recognition, image comparison, capture,
     * resizing, format conversion, watermark removal, and rectangle drawing.
     */
    class ImageTools
    {
    private:
        bool _saveBitmapToFile(HBITMAP hBitmap, const wchar_t* filename);
        bool isCaptureScreen = false;

        std::string _getImageNameFunc(const std::string& imgPath);
        std::string _getImageNameFunc(const std::string& imgPath, const std::string& name);
        int _getEncoderClsid(const WCHAR* format, CLSID* pClsid);

    public:
        // 获取模板图像在屏幕中的坐标
        // Finds the screen coordinates of a template image.
        ImagePosition getImagePosition(
                const std::string& filePath,
                double threshold = 1,
                bool notException = false);

        // 同 getImagePosition，但由调用方手动给出模板采集时电脑的缩放（1.5=150%）
        // Same as getImagePosition, but with a manually supplied template scale.
        ImagePosition getImagePositionZoom(
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false);

        // 便捷重载：缩放直接作为第 3 个参数传入（等价上面 ...Zoom 版本）。
        // templateZoom 必须写浮点数（如 1.5）；若写 1 / 0，会与 bool 版重载歧义而在编译期报错。
        ImagePosition getImagePosition(
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false);

        // 获取模板图像在屏幕中的所有匹配坐标
        // Finds all screen coordinates matching a template image.
        std::vector<ImagePosition> getImagePositionVec(
                const std::string& filePath,
                double threshold = 1,
                bool notException = false,
                int maxMatches = 0);

        // 同 getImagePositionVec，但由调用方手动给出模板采集时电脑的缩放
        // Same as getImagePositionVec, but with a manually supplied template scale.
        std::vector<ImagePosition> getImagePositionVecZoom(
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false,
                int maxMatches = 0);

        // 便捷重载：缩放直接作为第 3 个参数传入（等价上面 ...Zoom 版本）。
        std::vector<ImagePosition> getImagePositionVec(
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false,
                int maxMatches = 0);

        // 在指定屏幕区域内查找模板图像
        // Finds a template image within a specified screen region.
        ImagePosition getSpecifiedAreaImagePosition(
                int x1,
                int y1,
                int x2,
                int y2,
                const std::string& filePath,
                double threshold = 1,
                bool notException = false);

        // 同 getSpecifiedAreaImagePosition，但由调用方手动给出模板采集时电脑的缩放
        ImagePosition getSpecifiedAreaImagePositionZoom(
                int x1,
                int y1,
                int x2,
                int y2,
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false);

        // 便捷重载：缩放直接作为第 3 个参数传入（等价上面 ...Zoom 版本）。
        ImagePosition getSpecifiedAreaImagePosition(
                int x1,
                int y1,
                int x2,
                int y2,
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false);

        // 判断屏幕或指定区域中是否存在相似图像
        // Checks whether a similar image exists on the screen or in a specified region.
        bool findSimilarImage(
                const std::string& filePath,
                double threshold = 1.0);

        // 同 findSimilarImage（屏幕版），但由调用方手动给出模板采集时电脑的缩放
        bool findSimilarImageZoom(
                const std::string& filePath,
                double threshold,
                double templateZoom);

        // 便捷重载：缩放直接作为第 3 个参数传入（等价上面 ...Zoom 版本）。
        bool findSimilarImage(
                const std::string& filePath,
                double threshold,
                double templateZoom);

        bool findSpecifiedSimilarImage(
                int x1,
                int y1,
                int x2,
                int y2,
                const std::string& filePath,
                double threshold = 1);

        // 同 findSpecifiedSimilarImage，但由调用方手动给出模板采集时电脑的缩放
        bool findSpecifiedSimilarImageZoom(
                int x1,
                int y1,
                int x2,
                int y2,
                const std::string& filePath,
                double threshold,
                double templateZoom);

        // 便捷重载：缩放直接作为第 3 个参数传入（等价上面 ...Zoom 版本）。
        bool findSpecifiedSimilarImage(
                int x1,
                int y1,
                int x2,
                int y2,
                const std::string& filePath,
                double threshold,
                double templateZoom);

        // 直接比较两个图片文件
        // Compares two image files directly.
        bool findSimilarImage(
                const std::string& templatePath,
                const std::string& comparePath,
                double threshold = 1.0);

        // 转换图片格式
        // Converts an image to another format.
        bool setImageFormat(
                const std::string& imgPath,
                const std::string& targetImageFormat);

        // 全屏截图
        // Captures the full screen.
        bool captureScreen(const std::string& imageName);
        bool captureScreen(
                const std::string& imageName,
                const std::string& targetImageSavePath);

        // 修改图片尺寸，可覆盖原图或输出到指定路径
        // Resizes an image in place or writes the result to a specified path.
        bool setImageSize(
                const std::string& imgPath,
                float width,
                float height);

        bool setImageSize(
                const std::string& imgPath,
                float width,
                float height,
                const std::string& outPutName);

        bool setImageSize(
                const std::string& imgPath,
                float width,
                float height,
                const std::string& targetImageSavePath,
                const std::string& outPutName);

        // 获取图片尺寸和通道数
        // Reads image dimensions and channel count.
        ImageSize getImageSize(
                const std::string& filePath,
                bool outPutFlag = true);

        // 截取指定屏幕区域并保存
        // Captures a specified screen region and saves it.
        void captureAreaScreen(
                const std::string& saveName,
                int x1,
                int y1,
                int x2,
                int y2);

        // 去除指定矩形区域中的水印
        // Removes a watermark from a specified rectangle.
        bool removeWatermark(
                const std::string& imgPath,
                const std::string& outputPath,
                int topX,
                int topY,
                int btnX,
                int btnY);

        // 去除水印并返回 PNG 二进制数据
        // Removes a watermark and returns PNG binary data.
        std::vector<unsigned char> removeWatermarkBin(
                const std::string& imgPath,
                int topX,
                int topY,
                int btnX,
                int btnY);

        // 获取模板在主图中的矩形位置
        // Finds the rectangle of a template within a source image.
        ImageRect getImageRect(
                const std::string& filePath,
                double threshold = 1,
                bool notException = false);

        // 同 getImageRect（屏幕版），但由调用方手动给出模板采集时电脑的缩放
        ImageRect getImageRectZoom(
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false);

        // 便捷重载：缩放直接作为第 3 个参数传入（等价上面 ...Zoom 版本）。
        ImageRect getImageRect(
                const std::string& filePath,
                double threshold,
                double templateZoom,
                bool notException = false);

        ImageRect getImageRect(
                const std::string& templatePath,
                const std::string& filePath,
                double threshold = 1,
                bool notException = false);

        // 在图片中绘制矩形
        // Draws a rectangle on an image.
        bool drawRectOnImage(
                const std::string& imgPath,
                const std::string& outPutName,
                int topX,
                int topY,
                int btnX,
                int btnY);

        ~ImageTools();
    };

    /**
     * 视频处理类。
     *
     * 支持视频剪切、合并、添加音频、移除音轨、设置封面、读取时长和
     * 将图片帧序列合成为视频。
     * Supports trimming, merging, audio attachment, audio-track removal, cover images,
     * duration inspection, and image-frame sequence video creation.
     */
    class VideoTools
    {
    private:
        std::vector<std::string> tempPathList;

    public:
        VideoTools();

        // 按时间区间剪切视频
        // Trims a video by time range.
        bool videoArrivalTargetStop(
                const std::string& input_path,
                const std::string& output_path,
                double start_sec,
                double stop_sec,
                bool useFFMPEG = true,
                bool useGpu = false,
                bool show_errors = true);

        // 合并多个视频
        // Merges multiple videos.
        bool compositeVideo(
                const std::vector<std::string>& inputPaths,
                const std::string& outputPath,
                double fps = 0.0,
                bool useGpu = false);

        // 给视频添加音频
        // Adds an audio track to a video.
        bool addAudioToVideo(
                const std::string& inputPath,
                const std::string& audioPath,
                const std::string& outputPath,
                bool useGpu = false,
                bool silent = true);

        // 获取视频编码名称
        // Reads the video codec name.
        std::string getVideoCodec(const std::string& videoPath);

        // 删除视频音轨
        // Removes the audio track from a video.
        bool removeVideoAudioTrack(
                const std::string& inputPath,
                const std::string& outputPath,
                bool useGpu = false);

        // 设置视频封面
        // Sets a video cover image.
        bool setVideoCover(
                const std::string& videoPath,
                const std::string& imagePath,
                const std::string& outputPath,
                bool useGpu = false);

        // 读取视频时长，单位由现有实现决定
        // Reads video duration using the existing implementation's unit.
        long long getVideoDuration(const std::string& videoPath);

        // 获取视频帧率
        // Reads the video frame rate.
        double getVideoFrame(const std::string& videoPath);

        // 将内存中的图片帧合成为视频
        // Combines in-memory image frames into a video.
        bool imageFrameBinMergeVideo(
                std::vector<std::vector<unsigned char>>& video,
                const std::string& savePath,
                double fps = 30) noexcept;

        ~VideoTools();
    };

    /**
     * 音频处理类。
     *
     * 支持音频播放、暂停、停止、拼接、从视频分离音轨以及设置系统音量。
     * Supports audio playback, pause, stop, concatenation, audio-track extraction,
     * and system volume control.
     */
    class AudioTools
    {
    private:
        bool splitAudioTracksFlag = false;
        class Impl;
        std::unique_ptr<Impl> player_;

    public:
        AudioTools();

        // audioPath 使用 UTF-8 编码。
        // audioPath is UTF-8 encoded.
        explicit AudioTools(const std::string& audioPath);
        ~AudioTools();

        AudioTools(const AudioTools&) = delete;
        AudioTools& operator=(const AudioTools&) = delete;
        AudioTools(AudioTools&&) noexcept;
        AudioTools& operator=(AudioTools&&) noexcept;

        // 播放音频；已暂停时从当前位置继续。
        // Plays audio, or resumes from the current position when paused.
        bool playAudio();

        // 暂停音频。
        // Pauses audio.
        bool pauseAudio();

        // 停止音频并释放播放器资源。
        // Stops audio and releases player resources.
        bool stopAudio();

        // stopAudio 的兼容别名。
        // Compatibility alias for stopAudio.
        bool closeAudio();

        // 获取最近一次英文错误信息。
        // Returns the latest English error message.
        std::string getLastError() const;

        // 拼接多个音频文件
        // Concatenates multiple audio files.
        bool compositeMusic(
                const std::vector<std::string>& inputPaths,
                const std::string& outputPath,
                bool useGpu = false);

        // 从视频中分离音轨
        // Extracts an audio track from a video.
        bool splitAudioTracks(
                const std::string& videoPath,
                const std::string& audioSavePath,
                bool useGpu = false);

        // 设置系统音量，范围 0-100
        // Sets system volume in the range 0-100.
        bool setSystemVolumeModern(int volumePercent);
    };

}
