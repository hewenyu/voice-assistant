#include <gtest/gtest.h>
#include <audio/audio_capture.h>
#include <core/message_bus.h>
#include <core/message_types.h>
#include <core/subscriber.h>
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>
#include <cmath>

class AudioCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        message_bus_ = std::make_shared<core::MessageBus>();
        audio_capture_ = audio::IAudioCapture::CreateAudioCapture();
        ASSERT_TRUE(audio_capture_ != nullptr);
        audio_capture_->set_message_bus(message_bus_);
    }

    void TearDown() override {
        audio_capture_->stop_recording();
        audio_capture_.reset();
        message_bus_.reset();
    }

    std::shared_ptr<core::MessageBus> message_bus_;
    std::unique_ptr<audio::IAudioCapture> audio_capture_;
};

// 测试初始化
TEST_F(AudioCaptureTest, Initialize) {
    EXPECT_TRUE(audio_capture_->initialize());
}

// 测试音频格式
TEST_F(AudioCaptureTest, AudioFormat) {
    auto format = audio_capture_->get_audio_format();
    EXPECT_GT(format.sample_rate, 0);
    EXPECT_GT(format.channels, 0);
    EXPECT_GT(format.bits_per_sample, 0);
}

// 测试消息订阅和音频捕获
TEST_F(AudioCaptureTest, CaptureAndSubscribe) {
    bool received_start = false;
    bool received_data = false;
    bool received_stop = false;
    std::vector<float> audio_data;

    // 创建音频消息订阅者
    class AudioSubscriber : public core::ISubscriber {
    public:
        AudioSubscriber(bool& start, bool& data, bool& stop, std::vector<float>& buffer)
            : received_start_(start), received_data_(data), received_stop_(stop), audio_data_(buffer) {}

        std::string get_subscription_type() const override {
            return "audio";
        }

        void on_message(const std::shared_ptr<core::IMessage>& msg) override {
            auto audio_msg = std::dynamic_pointer_cast<core::AudioMessage>(msg);
            if (!audio_msg) return;

            switch (audio_msg->get_status()) {
                case core::AudioMessage::Status::Started:
                    received_start_ = true;
                    std::cout << "Received start message" << std::endl;
                    break;
                case core::AudioMessage::Status::Data:
                    received_data_ = true;
                    audio_data_ = audio_msg->get_data();
                    std::cout << "Received audio data, size: " << audio_data_.size() 
                             << ", total time: " << audio_data_.size() / 16000.0 << "s" << std::endl;
                    break;
                case core::AudioMessage::Status::Stopped:
                    received_stop_ = true;
                    std::cout << "Received stop message" << std::endl;
                    break;
                default:
                    break;
            }
        }

    private:
        bool& received_start_;
        bool& received_data_;
        bool& received_stop_;
        std::vector<float>& audio_data_;
    };

    auto subscriber = std::make_shared<AudioSubscriber>(received_start, received_data, received_stop, audio_data);
    message_bus_->subscribe("audio", subscriber);

    // 初始化并开始录音
    ASSERT_TRUE(audio_capture_->initialize());

    // 列出可用的音频源
    std::cout << "\nAvailable audio sources:" << std::endl;
    audio_capture_->list_applications();

    // 使用Edge浏览器的PID
    const unsigned int EDGE_PID = 31132;  // Edge浏览器的PID
    ASSERT_TRUE(audio_capture_->start_recording_application(EDGE_PID));

    // 等待30秒
    std::cout << "Recording for 30 seconds..." << std::endl;
    auto start_time = std::chrono::steady_clock::now();
    
    while (std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::steady_clock::now() - start_time).count() < 30) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Recording time: " 
                  << std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::steady_clock::now() - start_time).count() 
                  << "s" << std::endl;
    }

    // 停止录音
    std::cout << "Stopping recording..." << std::endl;
    audio_capture_->stop_recording();

    // 验证消息接收
    EXPECT_TRUE(received_start) << "Did not receive start message";
    EXPECT_TRUE(received_data) << "Did not receive any audio data";
    EXPECT_TRUE(received_stop) << "Did not receive stop message";
    
    if (received_data) {
        EXPECT_FALSE(audio_data.empty()) << "Received empty audio data";
        
        // 验证音频数据范围（浮点数应该在 [-1, 1] 范围内）
        for (float sample : audio_data) {
            EXPECT_GE(sample, -1.0f);
            EXPECT_LE(sample, 1.0f);
        }

        // 输出音频统计信息
        std::cout << "\nAudio Statistics:" << std::endl;
        std::cout << "Total samples: " << audio_data.size() << std::endl;
        std::cout << "Total time: " << audio_data.size() / 16000.0 << " seconds" << std::endl;
        
        // 计算音频电平
        float max_level = 0.0f;
        float sum_squares = 0.0f;
        for (float sample : audio_data) {
            max_level = std::max(max_level, std::abs(sample));
            sum_squares += sample * sample;
        }
        float rms_level = std::sqrt(sum_squares / audio_data.size());
        std::cout << "Max level: " << max_level << std::endl;
        std::cout << "RMS level: " << rms_level << std::endl;
    }
}

// 测试错误处理
TEST_F(AudioCaptureTest, ErrorHandling) {
    // 测试在未初始化时开始录音
    EXPECT_FALSE(audio_capture_->start_recording_application(0));

    // 测试无效的应用ID
    ASSERT_TRUE(audio_capture_->initialize());
    EXPECT_FALSE(audio_capture_->start_recording_application(999999));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 