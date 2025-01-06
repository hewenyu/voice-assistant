#include "core/speech_recognizer.h"
#include <stdexcept>
#include <sndfile.h>

namespace voice_assistant {

SpeechRecognizer::SpeechRecognizer() : context_(nullptr), initialized_(false) {}

SpeechRecognizer::~SpeechRecognizer() {
    if (context_) {
        // TODO: 添加适当的清理函数
        context_ = nullptr;
    }
}

bool SpeechRecognizer::initialize(const std::string& model_path) {
    try {
        // 初始化 SenseVoice 模型
        auto params = sense_voice_context_default_params();
        
        context_ = sense_voice_init_with_params_no_state(
            model_path.c_str(),
            params
        );
        
        if (!context_) {
            return false;
        }
        
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

    // 转换为double类型
    std::vector<double> samples(audio_data.begin(), audio_data.end());

    // 设置识别参数
    sense_voice_full_params params;
    params.language = config.language_code.c_str();
    
    // 执行识别
    int result = sense_voice_full_parallel(
        context_,
        params,
        samples,
        samples.size(),
        4  // 使用4个处理器
    );

    if (result != 0) {
        throw std::runtime_error("Recognition failed");
    }

    // TODO: 从context中获取识别结果
    return RecognitionResult{
        .transcript = "示例转录文本",  // 需要从context中获取实际结果
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