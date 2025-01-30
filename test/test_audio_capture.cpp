#include <gtest/gtest.h>
#include "../src/core/audio/audio_capture.h"
#include <thread>
#include <chrono>
#include <vector>

using namespace core::audio;

class AudioCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        capture = createAudioCapture();
        ASSERT_NE(capture, nullptr);
    }

    void TearDown() override {
        capture.reset();
    }

    std::unique_ptr<IAudioCapture> capture;
};

// 测试初始化
TEST_F(AudioCaptureTest, Initialize) {
    EXPECT_TRUE(capture->initialize());
}

// 测试音频格式
TEST_F(AudioCaptureTest, AudioFormat) {
    ASSERT_TRUE(capture->initialize());
    
    AudioFormat format;
    ASSERT_TRUE(capture->getFormat(format));
    
    // 验证格式是否符合预期（16kHz，单声道，16位）
    EXPECT_EQ(format.sample_rate, 16000);
    EXPECT_EQ(format.channels, 1);
    EXPECT_EQ(format.bits_per_sample, 16);
}

// 测试音频回调
TEST_F(AudioCaptureTest, AudioCallback) {
    ASSERT_TRUE(capture->initialize());

    bool callback_called = false;
    std::vector<float> captured_data;

    capture->setCallback([&](float* data, int frames) {
        callback_called = true;
        captured_data.insert(captured_data.end(), data, data + frames);
    });

    ASSERT_TRUE(capture->start());
    
    // 等待一段时间以接收音频数据
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    capture->stop();

    EXPECT_TRUE(callback_called);
    EXPECT_FALSE(captured_data.empty());
}

// 测试应用程序列表
TEST_F(AudioCaptureTest, ApplicationList) {
    ASSERT_TRUE(capture->initialize());

    const int MAX_APPS = 10;
    AudioAppInfo apps[MAX_APPS];
    int count = capture->getApplications(apps, MAX_APPS);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, MAX_APPS);

    // 验证每个应用程序信息
    for (int i = 0; i < count; ++i) {
        EXPECT_GT(apps[i].pid, 0);
        EXPECT_GT(wcslen(apps[i].name), 0);
    }
}

// 测试进程捕获
TEST_F(AudioCaptureTest, ProcessCapture) {
    ASSERT_TRUE(capture->initialize());

    // 获取应用程序列表
    const int MAX_APPS = 10;
    AudioAppInfo apps[MAX_APPS];
    int count = capture->getApplications(apps, MAX_APPS);

    if (count > 0) {
        // 尝试捕获第一个应用程序的音频
        EXPECT_TRUE(capture->startProcess(apps[0].pid));
        
        // 等待一段时间
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        capture->stop();
    }
}

// 测试启动和停止
TEST_F(AudioCaptureTest, StartStop) {
    ASSERT_TRUE(capture->initialize());

    EXPECT_TRUE(capture->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    capture->stop();

    // 测试多次启动停止
    EXPECT_TRUE(capture->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    capture->stop();

    EXPECT_TRUE(capture->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    capture->stop();
} 