#include <gtest/gtest.h>
#include "../src/core/recognizer/recognizer.h"
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <algorithm>

using namespace core::recognizer;
namespace fs = std::filesystem;

// 测试用的语言映射
const std::unordered_map<std::string, std::string> EXPECTED_TEXTS = {
    {"zh.wav", "你好世界"},   // 假设的中文内容
    {"en.wav", "hello world"}, // 假设的英文内容
    {"ja.wav", "こんにちは"}, // 假设的日语内容
    {"ko.wav", "안녕하세요"}, // 假设的韩语内容
    {"yue.wav", "你好"}       // 假设的粤语内容
};

class RecognizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        recognizer = createRecognizer();
        ASSERT_NE(recognizer, nullptr);

        // 设置基本配置
        config.model_path = "models/sherpa-onnx-zh-cn";
        config.lang = "zh";
        config.sample_rate = 16000;
        config.enable_vad = true;

        // 设置测试数据路径
        test_data_path = fs::path("test") / "test_data";
        ASSERT_TRUE(fs::exists(test_data_path)) << "测试数据目录不存在: " << test_data_path;
    }

    void TearDown() override {
        recognizer.reset();
    }

    // 辅助函数：从WAV文件加载音频数据
    std::vector<float> loadWavFile(const fs::path& file_path) {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开音频文件: " + file_path.string());
        }

        // 跳过WAV头部（44字节）
        file.seekg(44);

        // 获取数据部分大小
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(44, std::ios::beg);
        
        // 读取PCM数据
        std::vector<int16_t> pcm_data((file_size - 44) / sizeof(int16_t));
        file.read(reinterpret_cast<char*>(pcm_data.data()), file_size - 44);
        
        // 转换为float格式 (-1.0 到 1.0)
        std::vector<float> audio(pcm_data.size());
        for (size_t i = 0; i < pcm_data.size(); ++i) {
            audio[i] = pcm_data[i] / 32768.0f;
        }
        
        return audio;
    }

    // 辅助函数：获取测试音频文件列表
    std::vector<fs::path> getTestAudioFiles() {
        std::vector<fs::path> audio_files;
        for (const auto& entry : fs::directory_iterator(test_data_path)) {
            if (entry.path().extension() == ".wav") {
                audio_files.push_back(entry.path().filename());
            }
        }
        return audio_files;
    }

    std::unique_ptr<IRecognizer> recognizer;
    RecognizerConfig config;
    fs::path test_data_path;
};

// 测试初始化
TEST_F(RecognizerTest, Initialize) {
    EXPECT_TRUE(recognizer->initialize(config));
}

// 测试多语言音频识别
TEST_F(RecognizerTest, MultiLanguageRecognition) {
    ASSERT_TRUE(recognizer->initialize(config));
    auto test_files = getTestAudioFiles();
    ASSERT_FALSE(test_files.empty()) << "没有找到测试音频文件";

    for (const auto& test_file : test_files) {
        std::cout << "\n测试音频文件: " << test_file << std::endl;
        
        bool callback_called = false;
        std::string recognized_text;

        recognizer->setResultCallback([&](const RecognitionResult& result) {
            callback_called = true;
            recognized_text = result.text;
            std::cout << "识别结果: " << result.text 
                      << " (置信度: " << result.confidence << ")" << std::endl;
        });

        ASSERT_TRUE(recognizer->start());

        try {
            auto audio = loadWavFile(test_data_path / test_file);
            EXPECT_TRUE(recognizer->feedAudioData(audio.data(), audio.size()));
            
            // 等待识别结果
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            EXPECT_TRUE(callback_called) << "文件 " << test_file << " 没有产生识别结果";
            EXPECT_FALSE(recognized_text.empty()) << "文件 " << test_file << " 识别结果为空";
            
            // 如果有预期的文本，验证识别结果
            auto it = EXPECTED_TEXTS.find(test_file.string());
            if (it != EXPECTED_TEXTS.end()) {
                std::cout << "预期文本: " << it->second << std::endl;
                // 注意：这里使用包含关系而不是完全匹配，因为语音识别结果可能包含额外的词语
                EXPECT_TRUE(recognized_text.find(it->second) != std::string::npos)
                    << "识别结果与预期不符\n"
                    << "预期: " << it->second << "\n"
                    << "实际: " << recognized_text;
            }
            
        } catch (const std::exception& e) {
            ADD_FAILURE() << "处理文件 " << test_file << " 时发生错误: " << e.what();
        }

        recognizer->stop();
        recognizer->reset();
    }
}

// 测试语言支持
TEST_F(RecognizerTest, SupportedLanguages) {
    auto languages = recognizer->getSupportedLanguages();
    EXPECT_FALSE(languages.empty());
    
    // 验证是否支持所有测试文件的语言
    std::vector<std::string> required_languages = {"zh", "en", "ja", "ko"};
    for (const auto& lang : required_languages) {
        EXPECT_TRUE(std::find(languages.begin(), languages.end(), lang) != languages.end())
            << "不支持语言: " << lang;
    }
}

// 测试 VAD 功能
TEST_F(RecognizerTest, VoiceActivityDetection) {
    config.enable_vad = true;
    ASSERT_TRUE(recognizer->initialize(config));
    ASSERT_TRUE(recognizer->start());

    bool speech_detected = false;
    recognizer->setResultCallback([&](const RecognitionResult& result) {
        speech_detected = true;
        std::cout << "检测到语音活动: " << result.text << std::endl;
    });

    // 使用中文测试文件
    try {
        // 先发送静音
        std::vector<float> silence(16000, 0.0f);
        EXPECT_TRUE(recognizer->feedAudioData(silence.data(), silence.size()));
        
        // 然后发送中文音频
        auto audio = loadWavFile(test_data_path / "zh.wav");
        EXPECT_TRUE(recognizer->feedAudioData(audio.data(), audio.size()));
        
        // 等待处理
        std::this_thread::sleep_for(std::chrono::seconds(2));
        EXPECT_TRUE(speech_detected) << "VAD 没有检测到语音活动";
        
    } catch (const std::exception& e) {
        ADD_FAILURE() << "VAD 测试失败: " << e.what();
    }

    recognizer->stop();
}

// 测试错误恢复
TEST_F(RecognizerTest, ErrorRecovery) {
    ASSERT_TRUE(recognizer->initialize(config));
    auto test_files = getTestAudioFiles();

    if (!test_files.empty()) {
        ASSERT_TRUE(recognizer->start());

        // 模拟错误情况：发送不完整的数据
        try {
            auto audio = loadWavFile(test_data_path / test_files[0]);
            // 只发送一半的数据
            EXPECT_TRUE(recognizer->feedAudioData(audio.data(), audio.size() / 2));
            
            // 重置识别器
            recognizer->reset();
            
            // 重新发送完整数据
            EXPECT_TRUE(recognizer->feedAudioData(audio.data(), audio.size()));
            
        } catch (const std::exception& e) {
            ADD_FAILURE() << "测试错误恢复时发生错误: " << e.what();
        }

        recognizer->stop();
    }
}

// 测试重置功能
TEST_F(RecognizerTest, Reset) {
    ASSERT_TRUE(recognizer->initialize(config));
    ASSERT_TRUE(recognizer->start());

    auto audio = loadWavFile(test_data_path / "zh.wav");
    EXPECT_TRUE(recognizer->feedAudioData(audio.data(), audio.size()));

    recognizer->reset();
    EXPECT_TRUE(recognizer->feedAudioData(audio.data(), audio.size()));

    recognizer->stop();
}

// 测试错误情况
TEST_F(RecognizerTest, ErrorHandling) {
    // 测试无效的配置
    RecognizerConfig invalid_config;
    invalid_config.model_path = "non_existent_path";
    EXPECT_FALSE(recognizer->initialize(invalid_config));

    // 测试在未初始化时的操作
    EXPECT_FALSE(recognizer->start());
    
    auto audio = loadWavFile(test_data_path / "zh.wav");
    EXPECT_FALSE(recognizer->feedAudioData(audio.data(), audio.size()));
} 