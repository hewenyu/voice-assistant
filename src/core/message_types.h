#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>

namespace core {

// 基础消息接口
class IMessage {
public:
    virtual ~IMessage() = default;
    virtual std::string get_type() const = 0;
    virtual std::chrono::system_clock::time_point get_timestamp() const = 0;
};

// 音频数据消息
class AudioMessage : public IMessage {
public:
    enum class Status {
        Started,        // 开始录音
        Data,          // 音频数据
        Stopped,       // 停止录音
        Error          // 错误
    };

    AudioMessage(const std::vector<float>& data, int sample_rate, Status status)
        : data_(data), sample_rate_(sample_rate), status_(status),
          timestamp_(std::chrono::system_clock::now()) {}

    std::string get_type() const override { return "audio"; }
    std::chrono::system_clock::time_point get_timestamp() const override { return timestamp_; }

    const std::vector<float>& get_data() const { return data_; }
    int get_sample_rate() const { return sample_rate_; }
    Status get_status() const { return status_; }

private:
    std::vector<float> data_;
    int sample_rate_;
    Status status_;
    std::chrono::system_clock::time_point timestamp_;
};

// VAD消息
class VADMessage : public IMessage {
public:
    enum class Status {
        SpeechStart,    // 检测到语音开始
        SpeechEnd,      // 检测到语音结束
        NoSpeech        // 未检测到语音
    };

    VADMessage(Status status, float confidence = 0.0f)
        : status_(status), confidence_(confidence),
          timestamp_(std::chrono::system_clock::now()) {}

    std::string get_type() const override { return "vad"; }
    std::chrono::system_clock::time_point get_timestamp() const override { return timestamp_; }

    Status get_status() const { return status_; }
    float get_confidence() const { return confidence_; }

private:
    Status status_;
    float confidence_;
    std::chrono::system_clock::time_point timestamp_;
};

// ASR结果消息
class ASRMessage : public IMessage {
public:
    enum class Status {
        Started,        // 开始识别
        Partial,        // 部分结果
        Final,          // 最终结果
        Error          // 错误
    };

    ASRMessage(const std::string& text, Status status, 
               std::optional<float> confidence = std::nullopt,
               std::optional<std::string> language = std::nullopt)
        : text_(text), status_(status), 
          confidence_(confidence), language_(language),
          timestamp_(std::chrono::system_clock::now()) {}

    std::string get_type() const override { return "asr"; }
    std::chrono::system_clock::time_point get_timestamp() const override { return timestamp_; }

    const std::string& get_text() const { return text_; }
    Status get_status() const { return status_; }
    std::optional<float> get_confidence() const { return confidence_; }
    std::optional<std::string> get_language() const { return language_; }

private:
    std::string text_;
    Status status_;
    std::optional<float> confidence_;
    std::optional<std::string> language_;
    std::chrono::system_clock::time_point timestamp_;
};

// 翻译消息
class TranslationMessage : public IMessage {
public:
    enum class Status {
        Started,        // 开始翻译
        Completed,      // 翻译完成
        Error          // 错误
    };

    TranslationMessage(const std::string& source_text,
                      const std::string& translated_text,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      Status status,
                      const std::string& error_message = "")
        : source_text_(source_text), translated_text_(translated_text),
          source_lang_(source_lang), target_lang_(target_lang),
          status_(status), error_message_(error_message),
          timestamp_(std::chrono::system_clock::now()) {}

    std::string get_type() const override { return "translation"; }
    std::chrono::system_clock::time_point get_timestamp() const override { return timestamp_; }

    const std::string& get_source_text() const { return source_text_; }
    const std::string& get_translated_text() const { return translated_text_; }
    const std::string& get_source_lang() const { return source_lang_; }
    const std::string& get_target_lang() const { return target_lang_; }
    Status get_status() const { return status_; }
    const std::string& get_error_message() const { return error_message_; }

private:
    std::string source_text_;
    std::string translated_text_;
    std::string source_lang_;
    std::string target_lang_;
    Status status_;
    std::string error_message_;
    std::chrono::system_clock::time_point timestamp_;
};

// 字幕显示消息
class SubtitleMessage : public IMessage {
public:
    enum class Type {
        Original,       // 原始语音文本
        Translated     // 翻译后的文本
    };

    SubtitleMessage(const std::string& text, Type type,
                   bool is_final, int64_t segment_id)
        : text_(text), type_(type), is_final_(is_final),
          segment_id_(segment_id),
          timestamp_(std::chrono::system_clock::now()) {}

    std::string get_type() const override { return "subtitle"; }
    std::chrono::system_clock::time_point get_timestamp() const override { return timestamp_; }

    const std::string& get_text() const { return text_; }
    Type get_subtitle_type() const { return type_; }
    bool is_final() const { return is_final_; }
    int64_t get_segment_id() const { return segment_id_; }

private:
    std::string text_;
    Type type_;
    bool is_final_;
    int64_t segment_id_;
    std::chrono::system_clock::time_point timestamp_;
};

using MessagePtr = std::shared_ptr<IMessage>;

} // namespace core 