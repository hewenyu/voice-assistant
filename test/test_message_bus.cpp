#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include "core/message_bus.h"

using namespace core;
using namespace testing;

// Mock订阅者
class MockSubscriber : public ISubscriber {
public:
    MOCK_METHOD(void, on_message, (const MessagePtr&), (override));
    MOCK_METHOD(std::string, get_subscription_type, (), (const, override));
};

// 用于记录消息序列的订阅者
template<typename T>
class MessageCollector : public ISubscriber {
public:
    explicit MessageCollector(const std::string& type) : type_(type) {}

    void on_message(const MessagePtr& message) override {
        if (auto typed_message = std::dynamic_pointer_cast<T>(message)) {
            messages_.push(typed_message);
        }
    }

    std::string get_subscription_type() const override {
        return type_;
    }

    bool wait_for_message(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        auto start = std::chrono::steady_clock::now();
        while (messages_.empty()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    std::shared_ptr<T> get_message() {
        if (messages_.empty()) return nullptr;
        auto msg = messages_.front();
        messages_.pop();
        return msg;
    }

private:
    std::string type_;
    std::queue<std::shared_ptr<T>> messages_;
};

// 基本的发布订阅测试
TEST(MessageBusTest, BasicPublishSubscribe) {
    auto subscriber = std::make_shared<MockSubscriber>();
    EXPECT_CALL(*subscriber, get_subscription_type())
        .WillRepeatedly(Return("asr"));
    
    EXPECT_CALL(*subscriber, on_message(_))
        .Times(1);
    
    auto& bus = MessageBus::get_instance();
    bus.subscribe("asr", subscriber);
    
    auto message = std::make_shared<ASRMessage>("test", ASRMessage::Status::Final);
    bus.publish(message);

    // 等待消息处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// 音频消息测试
TEST(MessageBusTest, AudioMessageFlow) {
    auto collector = std::make_shared<MessageCollector<AudioMessage>>("audio");
    auto& bus = MessageBus::get_instance();
    bus.subscribe("audio", collector);

    // 测试音频流程：Started -> Data -> Stopped
    std::vector<float> audio_data = {1.0f, 2.0f, 3.0f};
    
    bus.publish(std::make_shared<AudioMessage>(
        std::vector<float>(), 16000, AudioMessage::Status::Started));
    
    bus.publish(std::make_shared<AudioMessage>(
        audio_data, 16000, AudioMessage::Status::Data));
    
    bus.publish(std::make_shared<AudioMessage>(
        std::vector<float>(), 16000, AudioMessage::Status::Stopped));

    // 验证消息序列
    ASSERT_TRUE(collector->wait_for_message());
    auto msg1 = collector->get_message();
    EXPECT_EQ(msg1->get_status(), AudioMessage::Status::Started);

    ASSERT_TRUE(collector->wait_for_message());
    auto msg2 = collector->get_message();
    EXPECT_EQ(msg2->get_status(), AudioMessage::Status::Data);
    EXPECT_EQ(msg2->get_data(), audio_data);

    ASSERT_TRUE(collector->wait_for_message());
    auto msg3 = collector->get_message();
    EXPECT_EQ(msg3->get_status(), AudioMessage::Status::Stopped);
}

// VAD消息测试
TEST(MessageBusTest, VADMessageFlow) {
    auto collector = std::make_shared<MessageCollector<VADMessage>>("vad");
    auto& bus = MessageBus::get_instance();
    bus.subscribe("vad", collector);

    // 测试VAD状态转换：NoSpeech -> SpeechStart -> SpeechEnd
    bus.publish(std::make_shared<VADMessage>(
        VADMessage::Status::NoSpeech, 0.1f));
    
    bus.publish(std::make_shared<VADMessage>(
        VADMessage::Status::SpeechStart, 0.8f));
    
    bus.publish(std::make_shared<VADMessage>(
        VADMessage::Status::SpeechEnd, 0.9f));

    // 验证消息序列
    ASSERT_TRUE(collector->wait_for_message());
    auto msg1 = collector->get_message();
    EXPECT_EQ(msg1->get_status(), VADMessage::Status::NoSpeech);
    EXPECT_FLOAT_EQ(msg1->get_confidence(), 0.1f);

    ASSERT_TRUE(collector->wait_for_message());
    auto msg2 = collector->get_message();
    EXPECT_EQ(msg2->get_status(), VADMessage::Status::SpeechStart);
    EXPECT_FLOAT_EQ(msg2->get_confidence(), 0.8f);

    ASSERT_TRUE(collector->wait_for_message());
    auto msg3 = collector->get_message();
    EXPECT_EQ(msg3->get_status(), VADMessage::Status::SpeechEnd);
    EXPECT_FLOAT_EQ(msg3->get_confidence(), 0.9f);
}

// ASR消息测试
TEST(MessageBusTest, ASRMessageFlow) {
    auto collector = std::make_shared<MessageCollector<ASRMessage>>("asr");
    auto& bus = MessageBus::get_instance();
    bus.subscribe("asr", collector);

    // 测试ASR状态转换：Started -> Partial -> Partial -> Final
    bus.publish(std::make_shared<ASRMessage>(
        "", ASRMessage::Status::Started));
    
    bus.publish(std::make_shared<ASRMessage>(
        "你", ASRMessage::Status::Partial, 0.7f, "zh"));
    
    bus.publish(std::make_shared<ASRMessage>(
        "你好", ASRMessage::Status::Partial, 0.8f, "zh"));
    
    bus.publish(std::make_shared<ASRMessage>(
        "你好世界", ASRMessage::Status::Final, 0.95f, "zh"));

    // 验证消息序列
    ASSERT_TRUE(collector->wait_for_message());
    auto msg1 = collector->get_message();
    EXPECT_EQ(msg1->get_status(), ASRMessage::Status::Started);

    ASSERT_TRUE(collector->wait_for_message());
    auto msg2 = collector->get_message();
    EXPECT_EQ(msg2->get_status(), ASRMessage::Status::Partial);
    EXPECT_EQ(msg2->get_text(), "你");

    ASSERT_TRUE(collector->wait_for_message());
    auto msg3 = collector->get_message();
    EXPECT_EQ(msg3->get_status(), ASRMessage::Status::Partial);
    EXPECT_EQ(msg3->get_text(), "你好");

    ASSERT_TRUE(collector->wait_for_message());
    auto msg4 = collector->get_message();
    EXPECT_EQ(msg4->get_status(), ASRMessage::Status::Final);
    EXPECT_EQ(msg4->get_text(), "你好世界");
    EXPECT_EQ(msg4->get_language().value(), "zh");
    EXPECT_FLOAT_EQ(msg4->get_confidence().value(), 0.95f);
}

// 翻译消息测试
TEST(MessageBusTest, TranslationMessageFlow) {
    auto collector = std::make_shared<MessageCollector<TranslationMessage>>("translation");
    auto& bus = MessageBus::get_instance();
    bus.subscribe("translation", collector);

    // 测试翻译流程：Started -> Completed
    bus.publish(std::make_shared<TranslationMessage>(
        "你好世界", "", "zh", "en",
        TranslationMessage::Status::Started));
    
    bus.publish(std::make_shared<TranslationMessage>(
        "你好世界", "Hello World", "zh", "en",
        TranslationMessage::Status::Completed));

    // 验证消息序列
    ASSERT_TRUE(collector->wait_for_message());
    auto msg1 = collector->get_message();
    EXPECT_EQ(msg1->get_status(), TranslationMessage::Status::Started);
    EXPECT_EQ(msg1->get_source_text(), "你好世界");

    ASSERT_TRUE(collector->wait_for_message());
    auto msg2 = collector->get_message();
    EXPECT_EQ(msg2->get_status(), TranslationMessage::Status::Completed);
    EXPECT_EQ(msg2->get_source_text(), "你好世界");
    EXPECT_EQ(msg2->get_translated_text(), "Hello World");
}

// 字幕消息测试
TEST(MessageBusTest, SubtitleMessageFlow) {
    auto collector = std::make_shared<MessageCollector<SubtitleMessage>>("subtitle");
    auto& bus = MessageBus::get_instance();
    bus.subscribe("subtitle", collector);

    // 测试字幕流程：原文和译文的配对
    int64_t segment_id = 1;
    
    bus.publish(std::make_shared<SubtitleMessage>(
        "你好世界", SubtitleMessage::Type::Original,
        false, segment_id));
    
    bus.publish(std::make_shared<SubtitleMessage>(
        "Hello World", SubtitleMessage::Type::Translated,
        true, segment_id));

    // 验证消息序列
    ASSERT_TRUE(collector->wait_for_message());
    auto msg1 = collector->get_message();
    EXPECT_EQ(msg1->get_subtitle_type(), SubtitleMessage::Type::Original);
    EXPECT_EQ(msg1->get_text(), "你好世界");
    EXPECT_FALSE(msg1->is_final());
    EXPECT_EQ(msg1->get_segment_id(), segment_id);

    ASSERT_TRUE(collector->wait_for_message());
    auto msg2 = collector->get_message();
    EXPECT_EQ(msg2->get_subtitle_type(), SubtitleMessage::Type::Translated);
    EXPECT_EQ(msg2->get_text(), "Hello World");
    EXPECT_TRUE(msg2->is_final());
    EXPECT_EQ(msg2->get_segment_id(), segment_id);
}

// 多线程测试
TEST(MessageBusTest, ThreadSafety) {
    const int num_threads = 10;
    const int messages_per_thread = 100;
    std::atomic<int> received_messages{0};
    
    auto subscriber = std::make_shared<CallbackSubscriber>(
        "asr",  // 修改为正确的消息类型
        [&received_messages](const MessagePtr&) {
            received_messages++;
        }
    );
    
    auto& bus = MessageBus::get_instance();
    bus.subscribe("asr", subscriber);  // 修改为正确的消息类型
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&bus, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                auto message = std::make_shared<ASRMessage>(
                    "test " + std::to_string(j), 
                    ASRMessage::Status::Partial);
                bus.publish(message);
                
                // 添加短暂延迟，避免消息处理过于密集
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 等待所有消息处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_EQ(received_messages, num_threads * messages_per_thread);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 