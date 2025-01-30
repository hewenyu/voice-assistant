#ifndef CORE_RECOGNIZER_H
#define CORE_RECOGNIZER_H

#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace core {
namespace recognizer {

// 识别结果结构体
struct RecognitionResult {
    std::string text;          // 识别的文本
    float confidence;          // 置信度
    bool is_final;            // 是否为最终结果
};

// 识别器配置结构体
struct RecognizerConfig {
    std::string model_path;    // 模型路径
    std::string lang;          // 语言
    int sample_rate;           // 采样率
    bool enable_vad;           // 是否启用VAD
};

// 语音识别接口类
class IRecognizer {
public:
    virtual ~IRecognizer() = default;

    // 初始化识别器
    virtual bool initialize(const RecognizerConfig& config) = 0;

    // 开始识别
    virtual bool start() = 0;

    // 停止识别
    virtual void stop() = 0;

    // 重置识别器状态
    virtual void reset() = 0;

    // 处理音频数据
    virtual bool feedAudioData(const float* audio_data, int num_samples) = 0;

    // 设置识别结果回调
    virtual void setResultCallback(std::function<void(const RecognitionResult&)> callback) = 0;

    // 获取支持的语言列表
    virtual std::vector<std::string> getSupportedLanguages() const = 0;
};

// 创建识别器实例
std::unique_ptr<IRecognizer> createRecognizer();

} // namespace recognizer
} // namespace core

#endif // CORE_RECOGNIZER_H 