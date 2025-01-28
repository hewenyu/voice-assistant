#pragma once

#include <memory>
#include <vector>
#include <common/model_config.h>
#include <core/message_bus.h>
#include <core/message_types.h>
#include <core/subscriber.h>
#include <recognizer/model_factory.h>
#include <sherpa-onnx/c-api/c-api.h>

namespace recognizer_data {

class IRecognizerData {
public:
    virtual ~IRecognizerData() = default;

    // 初始化识别器
    virtual bool Initialize(const common::ModelConfig& config) = 0;

    // 处理音频数据
    virtual void ProcessAudio(const std::vector<float>& audio_data, int sample_rate) = 0;

    // 重置识别器状态
    virtual void Reset() = 0;

    // 获取当前识别状态
    virtual bool IsActive() const = 0;

    // 订阅消息总线
    virtual void SubscribeToMessageBus(std::shared_ptr<core::MessageBus> message_bus) = 0;
};

} // namespace recognizer_data 