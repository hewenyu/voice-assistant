#include "core/speech_recognizer.h"
#include <stdexcept>
#include <sndfile.h>

namespace voice_assistant {

SpeechRecognizer::SpeechRecognizer() = default;
SpeechRecognizer::~SpeechRecognizer() = default;

bool SpeechRecognizer::initialize(const std::string& model_path) {
    try {
        // 初始化 SenseVoice 模型
        model_ = std::make_unique<sense_voice::SenseVoice>();
        // TODO: 设置模型参数并加载
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        // TODO: 添加错误日志
        return false;
    }
}

RecognitionResult SpeechRecognizer::recognize_file(
    const std::string& audio_path,
    const RecognitionConfig& config) {
    
    if (!initialized_) {
        throw std::runtime_error("Recognizer not initialized");
    }

    // 预处理音频
    auto audio_data = preprocess_audio(audio_path);
    return recognize_audio(audio_data, config);
}

RecognitionResult SpeechRecognizer::recognize_audio(
    const std::vector<float>& audio_data,
    const RecognitionConfig& config) {
    
    if (!initialized_) {
        throw std::runtime_error("Recognizer not initialized");
    }

    // TODO: 调用 SenseVoice 进行识别
    // 目前返回示例结果
    return RecognitionResult{
        .transcript = "示例转录文本",
        .confidence = 0.95f
    };
}

std::vector<float> SpeechRecognizer::preprocess_audio(const std::string& audio_path) {
    SF_INFO sf_info;
    SNDFILE* file = sf_open(audio_path.c_str(), SFM_READ, &sf_info);
    
    if (!file) {
        throw std::runtime_error("Failed to open audio file: " + audio_path);
    }

    std::vector<float> audio_data(sf_info.frames * sf_info.channels);
    sf_read_float(file, audio_data.data(), audio_data.size());
    sf_close(file);

    // TODO: 实现音频预处理
    // 1. 重采样到目标采样率
    // 2. 转换为单声道
    // 3. 应用预处理（如特征提取）

    return audio_data;
}

} // namespace voice_assistant 