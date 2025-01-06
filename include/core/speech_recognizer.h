#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <future>

// 避免宏冲突
#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

#include "sense-voice.h"

namespace voice_assistant {

// 识别状态
enum class RecognitionState {
    IDLE,           // 空闲状态
    RECOGNIZING,    // 正在识别
    FINISHED,       // 识别完成
    ERROR          // 发生错误
};

// 识别配置
struct RecognitionConfig {
    std::string encoding = "LINEAR16";              // 音频编码格式
    int sample_rate_hertz = 16000;                  // 采样率
    std::string language_code = "zh-CN";            // 语言代码
    bool enable_automatic_punctuation = true;        // 启用自动标点
    int max_alternatives = 1;                        // 最大备选数量
    bool profanity_filter = false;                  // 脏话过滤
    bool enable_word_time_offsets = false;          // 启用词时间戳
    
    // 语音上下文
    struct SpeechContext {
        std::vector<std::string> phrases;           // 提示词列表
        float boost = 1.0f;                         // 权重值
    };
    std::vector<SpeechContext> speech_contexts;     // 语音上下文列表
    
    // 音频源
    struct AudioSource {
        std::string content;                        // 音频内容（base64编码）
        std::string uri;                            // 音频URI
    };
    AudioSource audio;                              // 音频源
};

// 词时间戳
struct WordTimeOffset {
    std::string word;                               // 识别出的词
    double start_time;                              // 开始时间
    double end_time;                                // 结束时间
};

// 识别结果
struct RecognitionResult {
    std::string transcript;                         // 识别文本
    float confidence;                               // 置信度
    bool is_final;                                  // 是否为最终结果
    double start_time;                              // 开始时间
    double end_time;                                // 结束时间
    std::vector<WordTimeOffset> words;              // 词时间戳列表
};

// 流式识别的回调函数类型
using StreamingRecognitionCallback = std::function<void(const RecognitionResult&)>;

class SpeechRecognizer {
public:
    SpeechRecognizer();
    ~SpeechRecognizer();

    // 初始化识别器
    bool initialize(const std::string& model_path);

    // 同步识别 - 适用于短音频（<1分钟）
    RecognitionResult recognize_sync(const std::string& audio_path, 
                                   const RecognitionConfig& config = RecognitionConfig());
    
    RecognitionResult recognize_sync(const std::vector<float>& audio_data,
                                   const RecognitionConfig& config = RecognitionConfig());

    // 异步识别 - 适用于长音频（<480分钟）
    std::future<RecognitionResult> recognize_async(const std::string& audio_path,
                                                 const RecognitionConfig& config = RecognitionConfig());
    
    std::future<RecognitionResult> recognize_async(const std::vector<float>& audio_data,
                                                 const RecognitionConfig& config = RecognitionConfig());

    // 流式识别 - 适用于实时音频
    bool start_streaming(const RecognitionConfig& config,
                        StreamingRecognitionCallback callback);
    
    bool feed_audio(const std::vector<float>& audio_chunk);
    
    bool stop_streaming();

    // 获取当前状态
    RecognitionState get_state() const { return state_; }

private:
    // SenseVoice模型实例
    struct sense_voice_context* context_;
    bool initialized_ = false;
    RecognitionState state_ = RecognitionState::IDLE;

    // 流式识别相关
    StreamingRecognitionCallback streaming_callback_;
    bool is_streaming_ = false;
    std::vector<float> audio_buffer_;

    // 音频预处理
    std::vector<float> preprocess_audio(const std::string& audio_path);
    
    // 内部处理方法
    RecognitionResult process_recognition(const std::vector<float>& audio_data,
                                        const RecognitionConfig& config);
};

} // namespace voice_assistant 