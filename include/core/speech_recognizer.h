#pragma once

#include <string>
#include <memory>
#include <vector>
#include "sense-voice.h"

// 避免宏冲突
#undef MIN
#undef MAX

namespace voice_assistant {

struct RecognitionConfig {
    std::string encoding = "LINEAR16";
    int sample_rate_hertz = 16000;
    std::string language_code = "zh-CN";
    bool enable_automatic_punctuation = true;
    std::string model = "default";
};

struct RecognitionResult {
    std::string transcript;
    float confidence;
};

class SpeechRecognizer {
public:
    SpeechRecognizer();
    ~SpeechRecognizer();

    // 初始化识别器
    bool initialize(const std::string& model_path);

    // 从文件识别
    RecognitionResult recognize_file(const std::string& audio_path, 
                                   const RecognitionConfig& config = RecognitionConfig());

    // 从音频数据识别
    RecognitionResult recognize_audio(const std::vector<float>& audio_data,
                                    const RecognitionConfig& config = RecognitionConfig());

private:
    // SenseVoice模型实例
    struct sense_voice_context* context_;
    bool initialized_ = false;

    // 音频预处理
    std::vector<float> preprocess_audio(const std::string& audio_path);
};

} // namespace voice_assistant 