#include <winToolsH.hpp>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;
namespace fs = std::filesystem;


namespace wtl
{
    /************************* 视频时间 ****************************/
    VideoEvent::VideoEvent()
    {
        ;
    }

// FFMPEG版  //
// 剪切视频 //
    bool VideoEvent::videoArrivalTargetStop(const std::string& input_path, const std::string& output_path,
                                            double start_sec, double stop_sec, bool useFFMPEG, bool useGpu, bool show_errors)
    {
        if (useFFMPEG)
        {
            // FFMPEG版本

            // 计算持续时间
            double duration = stop_sec - start_sec;

            // 构建基础命令
            std::string cmd = "ffmpeg -loglevel " + std::string(show_errors ? "error" : "quiet") +
                              " -ss " + std::to_string(start_sec) +
                              " -i \"" + input_path + "\" -t " +
                              std::to_string(duration) + " -avoid_negative_ts 1 ";

            // 添加GPU加速判断
            if (useGpu) {
                // 使用GPU硬件编码（这里以NVIDIA NVENC为例）
                cmd += "-c:v h264_nvenc -preset fast ";
            } else {
                // 使用CPU复制流
                cmd += "-c copy ";
            }

            // 添加输出路径和覆盖选项
            cmd += "\"" + output_path + "\" -y";

            int result = _wsystem(fs::path(cmd).wstring().c_str());

            if (result != 0 && show_errors) {
                std::cerr << "FFmpeg command execution failed: " << cmd << std::endl;
            }

            return result == 0;
        }
        else
        {
            // OpenCV版本（保持原有逻辑，路径保持 UTF-8）
            std::string inputPath = input_path;
            std::string outputPath = output_path;

            // 第1步: 打开输入视频
            cv::VideoCapture cap(inputPath);
            if (!cap.isOpened()) {
                std::cerr << "Error: Unable to open input video: " << inputPath << std::endl;
                return false;
            }

            // 获取视频属性
            double fps = cap.get(cv::CAP_PROP_FPS);
            int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
            int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
            int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

            if (fps <= 0) {
                std::cerr << "Error: Unable to get video frame rate." << std::endl;
                return false;
            }

            // 计算视频总时长（秒）
            double video_duration = total_frames / fps;

            // 检查截断长度是否大于视频长度
            if (start_sec > video_duration) {
                std::cerr << "Error: Start time exceeds video duration. Video duration: "
                          << video_duration << " seconds, requested start: " << start_sec << " seconds." << std::endl;
                return false;
            }

            if (stop_sec > video_duration) {
                std::cerr << "Error: Stop time exceeds video duration. Video duration: "
                          << video_duration << " seconds, requested stop: " << stop_sec << " seconds." << std::endl;
                return false;
            }

            // 计算开始和结束帧索引
            int start_frame = static_cast<int>(start_sec * fps);
            int stop_frame = static_cast<int>(stop_sec * fps);

            // 确保开始和结束帧在有效范围内
            if (start_frame < 0) start_frame = 0;
            if (stop_frame > total_frames) stop_frame = total_frames;
            if (start_frame >= stop_frame) {
                std::cerr << "Error: Start time should be less than stop time, and stop frame should be greater than start frame." << std::endl;
                return false;
            }

            // 跳转到开始帧
            cap.set(cv::CAP_PROP_POS_FRAMES, start_frame);

            // 第2步: 创建VideoWriter以保存输出视频
            int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v'); // MP4编码
            cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(width, height));
            if (!writer.isOpened()) {
                std::cerr << "Error: Unable to create output video file: " << outputPath << std::endl;
                return false;
            }

            // 第3步: 逐帧读取并写入，直到结束帧
            cv::Mat frame;
            int current_frame = start_frame;
            while (current_frame <= stop_frame) {
                if (!cap.read(frame)) {
                    std::cerr << "Warning: Read failed at frame " << current_frame << ", terminating early." << std::endl;
                    break;
                }
                writer.write(frame);
                current_frame++;
            }

            // 第4步: 释放资源
            cap.release();
            writer.release();

            return true;
        }
    }

    static bool _compositeVideoCv(const std::vector<std::string>& inputPaths,
                                  const std::string& outputPath,
                                  double fps = 0.0)
    {
        // 路径保持 UTF-8（OpenCV 的 VideoCapture/VideoWriter 只接受 std::string）
        std::string output_path = outputPath;
        const std::vector<std::string>& converted_inputs = inputPaths;

        if (inputPaths.empty()) {
            return false;
        }

        std::vector<cv::VideoCapture> captures;

        // 1. 打开所有输入视频
        for (const auto& path : converted_inputs) {
            cv::VideoCapture cap(path);
            if (!cap.isOpened()) {
                return false;
            }
            captures.push_back(cap);
        }

        // 2. 获取第一个视频的参数作为基准
        int width = 0;
        int height = 0;
        double input_fps = 0.0;
        int fourcc = 0;

        // 获取第一个视频的参数
        cv::Mat firstFrame;
        captures[0] >> firstFrame;
        if (firstFrame.empty()) {
            return false;
        }

        // 重置第一个视频的位置，因为我们消耗了一帧
        captures[0].set(cv::CAP_PROP_POS_FRAMES, 0);

        width = firstFrame.cols;
        height = firstFrame.rows;
        input_fps = captures[0].get(cv::CAP_PROP_FPS);
        fourcc = static_cast<int>(captures[0].get(cv::CAP_PROP_FOURCC));

        // 获取视频的总帧数（可选，用于进度显示）
        std::vector<int> totalFrames;
        for (auto& cap : captures) {
            int frameCount = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
            totalFrames.push_back(frameCount);
        }

        // 3. 设置输出视频参数
        double output_fps = (fps > 0.0) ? fps : input_fps;
        if (output_fps <= 0) output_fps = 30.0;

        int output_width = width;  // 不进行水平拼接，只是顺序连接
        int output_height = height;

        // 4. 创建视频写入器
        cv::VideoWriter writer;
        if (fourcc == 0) {
            fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        }

        writer.open(output_path, fourcc, output_fps, cv::Size(output_width, output_height), true);
        if (!writer.isOpened()) {
            return false;
        }

        // 5. 顺序处理所有视频
        for (size_t videoIdx = 0; videoIdx < captures.size(); ++videoIdx) {
            cv::VideoCapture& cap = captures[videoIdx];

            // 重置当前视频到开始位置
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);

            while (true) {
                cv::Mat frame;
                cap >> frame;

                if (frame.empty()) {
                    // 当前视频已结束，跳出循环处理下一个视频
                    break;
                }

                // 将当前帧写入输出视频
                writer.write(frame);
            }

            // 重置捕获对象，准备处理下一个视频
            cap.release();
        }

        // 6. 释放资源
        writer.release();
        for (auto& cap : captures) {
            if (cap.isOpened()) {
                cap.release();
            }
        }

        return true;
    }

//////// 辅助部分 ////////
    static void _splitAudioTracks(const std::vector<std::string>& videoPath, const string& outputPath)
    {
        AudioEvent msc;
        string mp3_dir = wtl::format("{}/{}", outputPath, "temp_mp3");

        // 创建临时音频目录
        fs::create_directories(mp3_dir);

        // 存储生成的音频文件路径
        std::vector<std::string> audioPaths;

        for (size_t idx = 0; idx < videoPath.size(); ++idx)
        {
            // 生成唯一的音频文件名
            string audioFile = wtl::format("{}/audio_{:03d}.mp3", mp3_dir, idx);

            // 拆分音频
            if (msc.splitAudioTracks(videoPath[idx], audioFile))
            {
                audioPaths.push_back(audioFile);
            }
            else
            {
                // 如果拆分失败，记录错误
                wtl::println(wtl::Color("red"), "Audio splitting failed:", videoPath[idx]);
            }
        }

        if (!audioPaths.empty())
        {
            // 拼接所有音频文件
            string finalAudio = wtl::format("{}/final_audio.mp3", outputPath);
            bool compositeResult = msc.compositeMusic(audioPaths, finalAudio);

            if (compositeResult)
            {
                wtl::println(wtl::Color("green"), "Audio stitching completed:", finalAudio);
            }
            else
            {
                wtl::println(wtl::Color("red"), "Audio concatenation failed");
            }
        }
        else
        {
            wtl::println(wtl::Color("yellow"), "There are no audio files available for concatenation.");
        }

    }

// 辅助函数：获取视频编码格式
    std::string VideoEvent::getVideoCodec(const std::string& videoPath)
    {
        // 构建 ffprobe 命令获取编码信息
        std::string cmd = "ffprobe -v error -select_streams v:0 "
                          "-show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 \"";
        cmd += videoPath + "\"";

        std::array<char, 128> buffer;
        std::string result;
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) {
            return "unknown";
        }

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        _pclose(pipe);

        // 清理结果字符串
        result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
        result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());

        return result.empty() ? "unknown" : result;
    }


    static string _changeToTsFile(const std::vector<string> &inputPaths, const string &video_temp_dir)
    {
        VideoEvent vde;
        // 视频转化为ts格式
        std::vector<std::string> tsFiles;

        for (size_t i = 0; i < inputPaths.size(); ++i) {
            if (!fs::exists(inputPaths[i])) {
                // 文件不存在，跳过
                continue;
            }

            // 使用索引作为文件名，避免特殊字符问题
            std::string tsFileName = video_temp_dir + "/video_" + std::to_string(i) + ".ts";

            // 1. 获取视频编码格式 //
            std::string codec = vde.getVideoCodec(inputPaths[i]);
            std::string ffmpeg_cmd = "";

            // 2. 根据编码格式构建不同的命令
            if (codec == "h264" || codec == "avc" || codec == "avc1") {
                // H.264 编码，使用 h264_mp4toannexb
                ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                             "-bsf:v h264_mp4toannexb -f mpegts -y \"" + tsFileName + "\"";
            }
            else if (codec == "hevc" || codec == "h265") {
                // HEVC/H.265 编码，尝试使用 hevc_mp4toannexb
                ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                             "-bsf:v hevc_mp4toannexb -f mpegts -y \"" + tsFileName + "\"";
            }
            else if (codec == "mpeg4" || codec == "mpeg2video" || codec == "mpeg1video") {
                // MPEG 系列编码，不使用比特流过滤器
                ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                             "-f mpegts -y \"" + tsFileName + "\"";
            }
            else {
                // 其他未知编码，先尝试不使用过滤器
                ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                             "-f mpegts -y \"" + tsFileName + "\"";
            }

            // 3. 执行转换命令
            fs::path cmd = ffmpeg_cmd;
            int ret = _wsystem(cmd.wstring().c_str());

            // 4. 如果转换失败，尝试备用方案
            if (ret != 0) {
                // 方案A：对于 HEVC 编码，尝试不使用比特流过滤器
                if (codec == "hevc" || codec == "h265") {
                    ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" -c copy " +
                                 "-f mpegts -y \"" + tsFileName + "\"";
                    cmd = ffmpeg_cmd;
                    ret = _wsystem(cmd.wstring().c_str());
                }
                // 方案B：如果仍然失败，尝试重新编码（慢但兼容性最好）
                if (ret != 0) {
                    ffmpeg_cmd = "ffmpeg -loglevel error -i \"" + inputPaths[i] + "\" " +
                                 "-c:v libx264 -preset medium -c:a aac -f mpegts -y \"" + tsFileName + "\"";
                    cmd = ffmpeg_cmd;
                    ret = _wsystem(cmd.wstring().c_str());
                }

                if (ret != 0) {
                    // 转换失败，清理临时文件并返回
                    for (const auto& file : tsFiles) {
                        fs::remove(file);
                    }
                    return "";
                }
            }

            tsFiles.push_back(tsFileName);
        }

        if (tsFiles.empty()) return "";

        // 5. 构建 concat 字符串
        std::stringstream concatStream;
        concatStream << "concat:";
        for (size_t i = 0; i < tsFiles.size(); ++i) {
            concatStream << tsFiles[i];
            if (i != tsFiles.size() - 1) {
                concatStream << "|";
            }
        }
        std::string concatStr = concatStream.str();
        return concatStr;
    }

    // 合并视频 //
    bool VideoEvent::compositeVideo(const std::vector<std::string>& inputPaths,
                                    const std::string& outputPath,
                                    double fps,bool useGpu)
    {
        AudioEvent msc;
        // 临时音频视频路径
        vector<string> videoTempList;
        vector<string> musicTempList;

        /***** 生成临时文件目录 *****/
        string nowTime = wtl::getNowTime();
//    auto re_find_file_name = re::findAll(R"(\w+-\w+-\w+)",nowTime);
        auto re_find_file_name = re::sub("[-: ]","_",nowTime);
        string temp_dir_name = wtl::format("./temp_{}", re_find_file_name);
        fs::create_directories(temp_dir_name);

        // 开启线程拆分音频
        std::thread T1(_splitAudioTracks, inputPaths, temp_dir_name);
        T1.join();

        // 创建视频临时目录
        string video_temp_dir = wtl::format("{}/{}", temp_dir_name, "temp_video_ts");
        fs::create_directories(video_temp_dir);
        tempPathList.push_back(video_temp_dir);

#ifdef DEBUG
        println("compositeVideo函数: 继续执行!");
#endif

        // 转为Ts文件
        string allCmdToTsVideo = _changeToTsFile(inputPaths, video_temp_dir);

        // 4. 合并TS文件
        string save_ts_path = video_temp_dir + "/temp.mp4";
        std::string mergeCmd = "ffmpeg -loglevel error -i \"" + allCmdToTsVideo + "\" -c copy " +
                               "-bsf:a aac_adtstoasc -y \"" + save_ts_path + "\"";

        // 如果需要指定帧率
        if (fps > 0.0) {
            mergeCmd = "ffmpeg -loglevel error -i \"" + allCmdToTsVideo + "\" -c copy " +
                       "-r " + std::to_string(fps) + " " +
                       "-bsf:a aac_adtstoasc -y \"" + save_ts_path + "\"";
        }

        fs::path cmd_end = mergeCmd;
        bool result = _wsystem(cmd_end.wstring().c_str());

        // 如果处理失败返回false
        if (result != 0) {
            return false;
        }

        // 禁止视频的音频
        string end_temp_save = video_temp_dir + "/temp_end.mp4";
        bool end_save = removeVideoAudioTrack(save_ts_path, end_temp_save, useGpu);
        if (!end_save) {
            return false;
        }

        // 融合视频与音频
        // 注意：这里假设split_audio_tracks函数已经将音频合并为final_audio.mp3并保存在temp_dir_name目录下
        string final_audio_path = wtl::format("{}/final_audio.mp3", temp_dir_name);
        bool over = addAudioToVideo(end_temp_save, final_audio_path, outputPath, useGpu);
        wtl::FileManagement fl;
        fl.removeDir(temp_dir_name);
        return over;
    }

    bool VideoEvent::setVideoCover(const std::string& videoPath,
                                   const std::string& imagePath,
                                   const std::string& outputPath,bool useGpu)
    {
        try {
            // 检查输入文件是否存在
            if (!fs::exists(videoPath)) {
                std::cerr << "The video file does not exist.: " << videoPath << std::endl;
                return false;
            }

            if (!fs::exists(imagePath)) {
                std::cerr << "The cover image does not exist.: " << imagePath << std::endl;
                return false;
            }

            // 创建临时目录
            fs::create_directories("./temp");
            tempPathList.emplace_back("./temp");

            // 生成临时文件名
            fs::path filePath = videoPath;
            std::string stem = filePath.stem().string();
            std::string tempVideoPath = "./temp/" + stem + "_temp.mp4";

            // 构建第一个FFmpeg命令（复制视频）
            std::string removeCmd = "ffmpeg -y -i \"" + videoPath + "\" ";

            if (useGpu) {
                removeCmd += "-c:v h264_nvenc -preset fast -c:a copy ";
            } else {
                removeCmd += "-c copy ";
            }

            removeCmd += "-map 0 \"" + tempVideoPath + "\" 2>nul";

            // 构建第二个FFmpeg命令（添加封面）
            std::string addCmd = "ffmpeg -y -i \"" + tempVideoPath +
                                 "\" -i \"" + imagePath +
                                 "\" -map 0 -map 1 ";

            if (useGpu) {
                addCmd += "-c:v h264_nvenc -preset fast -c:a copy ";
            } else {
                addCmd += "-c copy ";
            }

            addCmd += "-disposition:v:1 attached_pic \"" + outputPath + "\" 2>nul";

            // 执行命令
            int ret1 = _wsystem(fs::path(removeCmd).wstring().c_str());
            int ret2 = _wsystem(fs::path(addCmd).wstring().c_str());

            // 清理临时文件
            fs::remove(tempVideoPath);
            fs::remove("./temp");

            return (ret1 == 0 && ret2 == 0);

        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << std::endl;
            return false;
        }
    }

    long long VideoEvent::getVideoDuration(const std::string& videoPath)
    {
        // 路径保持 UTF-8
        std::string video_path = videoPath;

        // 1. 打开视频文件
        cv::VideoCapture cap(video_path);

        // 检查是否成功打开
        if (!cap.isOpened()) {
            std::cerr << "Error: Could not open the video file: " << video_path << std::endl;
            return -1;
        }

        // 2. 获取关键属性
        // 总帧数
        long long totalFrameCount = static_cast<long long>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        // 帧率 (Frames Per Second)
        double fps = cap.get(cv::CAP_PROP_FPS);

        // 检查获取的值是否有效
        if (fps <= 0.0 || totalFrameCount <= 0) {
            // 尝试备选方案：通过读取直到结束来获取
            fps = cap.get(cv::CAP_PROP_FPS);
            if (fps <= 0.0) {
                // 使用默认帧率
                fps = 30.0;
            }

            // 如果总帧数不可用，尝试计算
            if (totalFrameCount <= 0) {
                // 回退方案：手动计算
                cap.set(cv::CAP_PROP_POS_AVI_RATIO, 1.0);
                double totalTime = cap.get(cv::CAP_PROP_POS_MSEC) / 1000.0; // 秒
                cap.set(cv::CAP_PROP_POS_AVI_RATIO, 0.0);

                if (totalTime > 0) {
                    totalFrameCount = static_cast<long long>(totalTime * fps);
                } else {
                    std::cerr << "Warning: Could not accurately determine video duration." << std::endl;
                    return -2; // 不同的错误码
                }
            }
        }

        // 3. 计算时长（秒）
        double totalDurationSeconds = static_cast<double>(totalFrameCount) / fps;

        // 转换为微秒（1秒 = 1,000,000微秒）
        long long durationInMicroseconds = static_cast<long long>(totalDurationSeconds * 1000000.0 + 0.5); // 四舍五入

        // 4. 输出调试信息（可选）
#ifdef DEBUG
        std::cout << "Video Information:" << std::endl;
    std::cout << "  Path: " << video_path << std::endl;
    std::cout << "  Total Frames: " << totalFrameCount << std::endl;
    std::cout << "  FPS: " << fps << std::endl;
    std::cout << "  Total Duration: " << totalDurationSeconds << " seconds" << std::endl;
    std::cout << "  Duration in microseconds: " << durationInMicroseconds << " µs" << std::endl;

    // 转换为可读格式
    int totalMinutes = static_cast<int>(totalDurationSeconds) / 60;
    int remainingSeconds = static_cast<int>(totalDurationSeconds) % 60;
    std::cout << "  Human Readable: " << totalMinutes << " minutes " << remainingSeconds << " seconds" << std::endl;
#endif

        // 释放资源
        cap.release();

        return durationInMicroseconds;
    }

    double VideoEvent::getVideoFrame(const std::string& videoPath)
    {
        // 路径保持 UTF-8
        std::string video_path = videoPath;

        VideoCapture cap(video_path);
        if (!cap.isOpened())
        {
            std::cerr << "Error: Could not open video file." << std::endl;
            return 0.0;
        }

        // 获取帧率（fps）
        double fps = cap.get(CAP_PROP_FPS);

        cap.release();
        return fps;
    }


// 组合视频帧数 //
    bool VideoEvent::imageFrameBinMergeVideo(
            std::vector<std::vector<unsigned char>>&video,
            const std::string &savePath,
            double fps
    ) noexcept
    {
        // 路径保持 UTF-8
        std::string save_path = savePath;

        if (video.empty())
        {
            cerr << "Error: Input video data is empty!" << endl;
            return false;
        }

        vector<Mat> frames;
        for (const auto& frameData : video)
        {
            if (frameData.empty())
            {
                cerr << "Warning: Empty frame data, skip." << endl;
                continue;
            }

            Mat img = imdecode(frameData, IMREAD_COLOR);
            if (img.empty())
            {
                cerr << "Warning: Failed to decode frame, skip this frame." << endl;
                continue;
            }
            frames.push_back(img);
        }

        if (frames.empty())
        {
            cerr << "Error: No valid frames decoded!" << endl;
            return false;
        }

        Size frameSize = frames[0].size();

        int fourcc = VideoWriter::fourcc('M', 'P', '4', 'V');
        // ======== 修改1：使用 fps 参数 ========
        VideoWriter writer(save_path, fourcc, fps, frameSize, true);

        if (!writer.isOpened()) {
            fourcc = VideoWriter::fourcc('X', 'V', 'I', 'D');
            // ======== 修改2：使用 fps 参数 ========
            writer.open(save_path, fourcc, fps, frameSize, true);
            if (!writer.isOpened()) {
                cerr << "Error: Could not open video writer for path: " << save_path << endl;
                return false;
            }
        }

        for (const auto& frame : frames)
        {
            writer.write(frame);
        }

        writer.release();
        return true;
    }

    bool VideoEvent::removeVideoAudioTrack(const std::string &inputPath, const std::string &outputPath,bool useGpu)
    {
        std::string ffmpeg_cmd = "ffmpeg -hide_banner -loglevel error -y -i \"" + inputPath + "\" ";

        // 添加GPU加速判断
        if (useGpu) {
            // 使用GPU编码
            ffmpeg_cmd += "-c:v h264_nvenc -preset fast -an \"" + outputPath + "\"";
        } else {
            // 复制视频流
            ffmpeg_cmd += "-c:v copy -an \"" + outputPath + "\"";
        }

        return _wsystem(fs::path(ffmpeg_cmd).wstring().c_str()) == 0;
    }

    bool VideoEvent::addAudioToVideo(const string &inputPath, const string &audioPath, const string &outputPath,bool useGpu, bool silent)
    {
        // 基础命令构建
        std::string ffmpeg_cmd = "ffmpeg ";

        // 添加日志级别
        if (silent) {
            ffmpeg_cmd += "-hide_banner -loglevel error ";
        } else {
            ffmpeg_cmd += "-loglevel error ";
        }

        // 添加输入文件
        ffmpeg_cmd += "-i \"" + inputPath + "\" ";
        ffmpeg_cmd += "-i \"" + audioPath + "\" ";

        // 添加GPU加速判断
        if (useGpu) {
            // 使用GPU编码视频流
            ffmpeg_cmd += "-c:v h264_nvenc -preset fast ";
        } else {
            // 使用默认编码
            ffmpeg_cmd += "-c copy ";
        }

        // 添加音频处理和输出
        ffmpeg_cmd += "-map 0:v:0 -map 1:a:0 ";
        ffmpeg_cmd += "-y \"" + outputPath + "\"";

        return _wsystem(fs::path(ffmpeg_cmd).wstring().c_str()) == 0;
    }

    VideoEvent::~VideoEvent()
    {
        wtl::FileManagement fl;
        for (auto &i:tempPathList)
        {
            try
            {
                fl.removeDir(i);
            }
            catch (...) {}
        }
    }
};
