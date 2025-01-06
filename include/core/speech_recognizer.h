#pragma once

// 避免宏冲突
#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <functional>
#include "csrc/sense-voice.h"

namespace voice_assistant {

// 音频配置
struct AudioConfig {
    std::string content;  // base64编码的音频数据
    std::string uri;      // 音频文件URI
};

// 识别配置
struct RecognitionConfig {
    std::string encoding = "LINEAR16";
    int32_t sample_rate_hertz = 16000;
    std::string language_code = "zh-CN";
    bool enable_automatic_punctuation = true;
    int32_t max_alternatives = 1;
    bool profanity_filter = false;
    bool enable_word_time_offsets = false;
    
    // 语音上下文
    struct SpeechContext {
        std::vector<std::string> phrases;  // 提示词列表
        float boost = 1.0f;                // 权重值
    };
    std::vector<SpeechContext> speech_contexts;  // 语音上下文列表
    
    AudioConfig audio;
};

// 词时间戳
struct Word {
    std::string word;
    double start_time;
    double end_time;
};

// 识别结果
struct RecognitionResult {
    std::string transcript;
    double confidence;
    bool is_final;
    std::vector<Word> words;
};

// 识别状态
enum class RecognitionState {
    IDLE,
    RECOGNIZING,
    FINISHED,
    ERROR
};

// 流式识别回调函数类型
using StreamingRecognitionCallback = std::function<void(const RecognitionResult&)>;

class SpeechRecognizer {
public:
    SpeechRecognizer();
    ~SpeechRecognizer();

    // 初始化
    bool initialize(const std::string& model_path);

    // 同步识别
    RecognitionResult recognize_sync(const std::string& audio_path, const RecognitionConfig& config);
    RecognitionResult recognize_sync(const std::vector<float>& audio_data, const RecognitionConfig& config);

    // 异步识别
    std::future<RecognitionResult> recognize_async(const std::string& audio_path, const RecognitionConfig& config);
    std::future<RecognitionResult> recognize_async(const std::vector<float>& audio_data, const RecognitionConfig& config);

    // 流式识别
    bool start_streaming(const RecognitionConfig& config, StreamingRecognitionCallback callback);
    bool feed_audio(const std::vector<float>& audio_chunk);
    bool stop_streaming();

    // 获取状态
    RecognitionState get_state() const { return state_; }

private:
    // 内部处理方法
    RecognitionResult process_recognition(const std::vector<float>& audio_data, const RecognitionConfig& config);
    std::vector<float> preprocess_audio(const std::string& audio_path);

    sense_voice_context* context_;
    bool initialized_;
    bool is_streaming_;
    RecognitionState state_;
    StreamingRecognitionCallback streaming_callback_;
    std::vector<float> audio_buffer_;
};

} // namespace voice_assistant 