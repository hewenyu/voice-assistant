#pragma once

#include <string>
#include <vector>
#include <memory>
#include "common/model_config.h"

#ifdef __linux__
#include "linux_pulease/pulse_audio_capture.h"
#endif

#ifdef _WIN32
#include "windows/wasapi_audio_capture.h"
#endif

namespace audio {

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    // 初始化音频捕获
    virtual bool initialize() = 0;

    // 开始录制指定应用的音频
    virtual bool start_recording_application(uint32_t app_id) = 0;

    // 停止录制
    virtual void stop_recording() = 0;

    // 列出可用的音频应用
    virtual void list_applications() = 0;

    // 工厂方法创建平台特定实现
    // LINUX 平台特定实现 USE PulseAudio create
    // WINDOWS 平台特定实现 USE WASAPI create
    static std::unique_ptr<IAudioCapture> create(const common::ModelConfig& config);
    
};



// 



} // namespace voice 