#include "recognizer_data.h"
#include <iostream>

namespace recognizer_data {

class RecognizerData : public IRecognizerData {
private:
    std::shared_ptr<core::MessageBus> message_bus_;
    const SherpaOnnxOfflineRecognizer* recognizer_;
    SherpaOnnxVoiceActivityDetector* vad_;
    bool is_active_;
    std::vector<float> audio_buffer_;

public:
    RecognizerData() : recognizer_(nullptr), vad_(nullptr), is_active_(false) {}

    ~RecognizerData() {
        if (recognizer_) {
            SherpaOnnxDestroyOfflineRecognizer(recognizer_);
        }
        if (vad_) {
            SherpaOnnxDestroyVoiceActivityDetector(vad_);
        }
    }

    bool Initialize(const common::ModelConfig& config) override {
        try {
            // 创建识别器
            recognizer_ = recognizer::ModelFactory::CreateModel(config);
            if (!recognizer_) {
                std::cerr << "Failed to create speech recognizer." << std::endl;
                return false;
            }

            // 创建VAD
            vad_ = recognizer::ModelFactory::CreateVoiceActivityDetector(config);
            if (!vad_) {
                std::cerr << "Failed to create VAD." << std::endl;
                SherpaOnnxDestroyOfflineRecognizer(recognizer_);
                recognizer_ = nullptr;
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error initializing recognizer: " << e.what() << std::endl;
            return false;
        }
    }

    void ProcessAudio(const std::vector<float>& audio_data, int sample_rate) override {
        if (!recognizer_ || !vad_ || !message_bus_) {
            return;
        }

        // 处理VAD
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad_, audio_data.data(), audio_data.size());
        int is_speech = SherpaOnnxVoiceActivityDetectorDetected(vad_);
        float speech_confidence = is_speech ? 1.0f : 0.0f;

        // 发送VAD消息
        auto vad_status = is_speech ? core::VADMessage::Status::SpeechStart : core::VADMessage::Status::NoSpeech;
        auto vad_msg = std::make_shared<core::VADMessage>(vad_status, speech_confidence);
        message_bus_->publish(vad_msg);

        if (is_speech) {
            // 将音频数据添加到缓冲区
            audio_buffer_.insert(audio_buffer_.end(), audio_data.begin(), audio_data.end());
            is_active_ = true;

            // 创建识别流
            auto stream = SherpaOnnxCreateOfflineStream(recognizer_);
            if (!stream) {
                return;
            }

            // 处理音频数据
            SherpaOnnxAcceptWaveformOffline(stream, sample_rate, audio_buffer_.data(), audio_buffer_.size());
            
            // 解码
            SherpaOnnxDecodeOfflineStream(recognizer_, stream);
            
            // 获取结果
            const auto* result = SherpaOnnxGetOfflineStreamResult(stream);

            if (result) {
                // 发送ASR消息
                auto asr_msg = std::make_shared<core::ASRMessage>(
                    result->text,
                    core::ASRMessage::Status::Partial
                );
                message_bus_->publish(asr_msg);

                SherpaOnnxDestroyOfflineRecognizerResult(result);
            }

            SherpaOnnxDestroyOfflineStream(stream);
        } else if (is_active_) {
            // 语音结束，发送最终结果
            if (!audio_buffer_.empty()) {
                auto stream = SherpaOnnxCreateOfflineStream(recognizer_);
                if (stream) {
                    SherpaOnnxAcceptWaveformOffline(stream, sample_rate, audio_buffer_.data(), audio_buffer_.size());
                    
                    // 解码
                    SherpaOnnxDecodeOfflineStream(recognizer_, stream);
                    
                    // 获取结果
                    const auto* result = SherpaOnnxGetOfflineStreamResult(stream);

                    if (result) {
                        auto asr_msg = std::make_shared<core::ASRMessage>(
                            result->text,
                            core::ASRMessage::Status::Final
                        );
                        message_bus_->publish(asr_msg);

                        SherpaOnnxDestroyOfflineRecognizerResult(result);
                    }

                    SherpaOnnxDestroyOfflineStream(stream);
                }
            }

            // 重置状态
            Reset();
        }
    }

    void Reset() override {
        audio_buffer_.clear();
        is_active_ = false;
    }

    bool IsActive() const override {
        return is_active_;
    }

    void SubscribeToMessageBus(std::shared_ptr<core::MessageBus> message_bus) override {
        message_bus_ = message_bus;
        
        // 创建音频消息处理函数
        auto audio_handler = [this](const core::MessagePtr& msg) {
            if (auto audio_msg = std::dynamic_pointer_cast<core::AudioMessage>(msg)) {
                if (audio_msg->get_status() == core::AudioMessage::Status::Data) {
                    this->ProcessAudio(audio_msg->get_data(), audio_msg->get_sample_rate());
                } else if (audio_msg->get_status() == core::AudioMessage::Status::Stopped) {
                    this->Reset();
                }
            }
        };

        // 订阅音频消息
        message_bus_->subscribe("audio", std::make_shared<core::CallbackSubscriber>("audio", audio_handler));
    }
};

// 工厂函数
std::unique_ptr<IRecognizerData> CreateRecognizerData() {
    return std::make_unique<RecognizerData>();
}

} // namespace recognizer_data 