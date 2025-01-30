#ifndef CORE_AUDIO_CAPTURE_H
#define CORE_AUDIO_CAPTURE_H

#include <functional>
#include <memory>

namespace core {
namespace audio {

// 音频格式结构体
struct AudioFormat {
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int bits_per_sample;
};

// 应用程序信息结构体
struct AudioAppInfo {
    unsigned int pid;
    wchar_t name[260]; // MAX_PATH
};

// 音频捕获接口类
class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    // 初始化音频捕获
    virtual bool initialize() = 0;

    // 开始捕获
    virtual bool start() = 0;

    // 停止捕获
    virtual void stop() = 0;

    // 设置音频数据回调
    virtual void setCallback(std::function<void(float*, int)> callback) = 0;

    // 获取音频格式
    virtual bool getFormat(AudioFormat& format) = 0;

    // 获取应用程序列表
    virtual int getApplications(AudioAppInfo* apps, int maxCount) = 0;

    // 开始捕获特定进程的音频
    virtual bool startProcess(unsigned int pid) = 0;
};

// 音频捕获工厂函数
std::unique_ptr<IAudioCapture> createAudioCapture();

} // namespace audio
} // namespace core

#endif // CORE_AUDIO_CAPTURE_H 