#include <winToolsH.hpp>

namespace wtl
{
    /************************* 音频操作 ****************************/
    bool AudioEvent::compositeMusic(const std::vector<std::string>& inputPaths, const std::string& outputPath,bool useGpu)
    {
        if (inputPaths.empty() || outputPath.empty()) {
            return false;
        }

        // 方法2：使用concat滤镜但先统一格式
        std::stringstream cmd;
        cmd << "ffmpeg -v error -y ";

        // 添加所有输入文件
        for (const auto& input : inputPaths) {
            cmd << "-i \"" << input << "\" ";
        }

        // 为每个输入添加aformat滤镜统一格式
        cmd << "-filter_complex \"";
        for (size_t i = 0; i < inputPaths.size(); ++i) {
            cmd << "[" << i << ":a]aformat=sample_rates=44100:channel_layouts=stereo[a" << i << "]; ";
        }

        // 拼接统一格式后的音频
        for (size_t i = 0; i < inputPaths.size(); ++i) {
            cmd << "[a" << i << "]";
        }
        cmd << "concat=n=" << inputPaths.size() << ":v=0:a=1[out]\" ";

        // 音频编码设置
        cmd << "-map \"[out]\" -c:a libmp3lame -b:a 192k ";

        // 虽然音频处理GPU加速不明显，但保持参数一致性
        if (useGpu) {
            cmd << "-c:v h264_nvenc ";  // 如果有视频流的情况下
        }

        cmd << "\"" << outputPath << "\"";

        fs::path command = cmd.str();

#ifdef DEBUG
        println(Color("yellow"), "ffmpeg命令:", command.string());
#endif

        int result = _wsystem(command.wstring().c_str());
        return (result == 0);
    }

    bool AudioEvent::splitAudioTracks(const std::string &videoPath, const std::string &audioSavePath,bool useGpu)
    {
        // 构建命令
        std::string cmd = "ffmpeg -hide_banner -loglevel quiet -y -i \"" + videoPath + "\" -vn ";

        // 音频编码设置
        cmd += "-acodec libmp3lame -q:a 4 ";

        // 虽然音频分离不太需要GPU加速，但保持参数一致性
        if (useGpu) {
            cmd += "-c:v h264_nvenc ";  // 如果有视频流的情况下
        }

        cmd += "\"" + audioSavePath + "\"";

        int result = _wsystem(fs::path(cmd).wstring().c_str());

        // system/_wsystem 调用成功返回0
        return (result == 0);
    }

    // 设置音频音量 //
    bool AudioEvent::setSystemVolumeModern(int volumePercent)
    {
        if (volumePercent < 0) volumePercent = 0;
        if (volumePercent > 100) volumePercent = 100;

        HRESULT hr = S_OK;
        CoInitialize(NULL);

        // 创建设备枚举器
        IMMDeviceEnumerator* pEnumerator = NULL;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                              CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                              (void**)&pEnumerator);
        if (FAILED(hr)) {
            CoUninitialize();
            return false;
        }

        // 获取默认音频端点
        IMMDevice* pDevice = NULL;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr)) {
            pEnumerator->Release();
            CoUninitialize();
            return false;
        }

        // 激活音量控制接口
        IAudioEndpointVolume* pEndpointVolume = NULL;
        hr = pDevice->Activate(__uuidof(IAudioEndpointVolume),
                               CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
        if (FAILED(hr)) {
            pDevice->Release();
            pEnumerator->Release();
            CoUninitialize();
            return false;
        }

        // 设置音量（0.0到1.0之间）
        float volumeLevel = volumePercent / 100.0f;
        hr = pEndpointVolume->SetMasterVolumeLevelScalar(volumeLevel, NULL);

        // 静音（可选）
        // hr = pEndpointVolume->SetMute(TRUE, NULL);  // 静音
        // hr = pEndpointVolume->SetMute(FALSE, NULL); // 取消静音

        // 清理资源
        pEndpointVolume->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();

        return SUCCEEDED(hr);
    }
}
