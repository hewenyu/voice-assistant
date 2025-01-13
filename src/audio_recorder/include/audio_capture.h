#pragma once

#include <string>
#include <vector>
#include <memory>
#include "audio_format.h"

namespace voice {

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    // 初始化音频捕获
    virtual bool initialize() = 0;

    // 开始录制指定应用的音频
    virtual bool start_recording_application(uint32_t app_id, const std::string& output_path = "") = 0;

    // 停止录制
    virtual void stop_recording() = 0;

    // 列出可用的音频应用
    virtual void list_applications() = 0;

    // 工厂方法创建平台特定实现
    static std::unique_ptr<IAudioCapture> create(const std::string& config_path = "");
};

} // namespace voice 