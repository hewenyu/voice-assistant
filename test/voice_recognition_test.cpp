#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

using voice::VoiceService;
using voice::SyncRecognizeRequest;
using voice::SyncRecognizeResponse;
using voice::RecognitionConfig;
using voice::AudioEncoding;

class VoiceRecognitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建gRPC channel和stub
        channel_ = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
        stub_ = VoiceService::NewStub(channel_);

        // 等待服务器就绪
        std::cout << "Waiting for server to be ready..." << std::endl;
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        bool connected = channel_->WaitForConnected(deadline);
        ASSERT_TRUE(connected) << "Failed to connect to server. Is the server running?";
        std::cout << "Server is ready." << std::endl;
    }

    std::string ReadFile(const char* filename) {
        // 获取项目根目录
        const char* workspace_dir = std::getenv("WORKSPACE_DIR");
        if (!workspace_dir) {
            ADD_FAILURE() << "WORKSPACE_DIR environment variable not set";
            return "";
        }

        // 构建完整的文件路径
        std::string full_path = std::string(workspace_dir) + "/" + filename;
        std::cout << "Reading file: " << full_path << std::endl;

        std::ifstream file(full_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            ADD_FAILURE() << "Failed to open file: " << full_path;
            return "";
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string buffer;
        buffer.resize(size);
        if (!file.read(const_cast<char*>(buffer.data()), size)) {
            ADD_FAILURE() << "Failed to read file: " << full_path;
            return "";
        }

        return buffer;
    }

    bool TestRecognition(const char* filename, const std::string& expected_text) {
        std::string audio_data = ReadFile(filename);
        if (audio_data.empty()) {
            return false;
        }

        SyncRecognizeRequest request;
        auto* config = request.mutable_config();
        config->set_encoding(AudioEncoding::LINEAR16);
        config->set_sample_rate_hertz(16000);
        config->set_language_code("auto");
        request.set_audio_content(audio_data);

        SyncRecognizeResponse response;
        grpc::ClientContext context;
        grpc::Status status = stub_->SyncRecognize(&context, request, &response);

        EXPECT_TRUE(status.ok()) << "RPC failed: " << status.error_message();
        if (!status.ok()) {
            return false;
        }

        std::cout << "\nTesting file: " << filename << std::endl;
        
        if (response.results_size() > 0 && response.results(0).alternatives_size() > 0) {
            std::string result = response.results(0).alternatives(0).transcript();
            std::cout << "Got result: " << result << std::endl;
            std::cout << "Expected to contain: " << expected_text << std::endl;

            // 检查识别结果是否包含预期文本
            bool contains_expected = result.find(expected_text) != std::string::npos;
            EXPECT_TRUE(contains_expected) 
                << "Expected text not found in recognition result.\n"
                << "Expected to contain: " << expected_text << "\n"
                << "Got: " << result;

            return contains_expected;
        }

        std::cout << "No recognition results received" << std::endl;
        return false;
    }

    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<VoiceService::Stub> stub_;
};

TEST_F(VoiceRecognitionTest, EnglishRecognition) {
    EXPECT_TRUE(TestRecognition(
        "test/test_data/en.wav",
        "The tribal chieftain called for the boy and presented him with 50 pieces of gold"
    ));
}

TEST_F(VoiceRecognitionTest, ChineseRecognition) {
    EXPECT_TRUE(TestRecognition(
        "test/test_data/zh.wav",
        "开放时间早上9点至下午5点"
    ));
}

TEST_F(VoiceRecognitionTest, JapaneseRecognition) {
    EXPECT_TRUE(TestRecognition(
        "test/test_data/ja.wav",
        "うちの中学は弁当制で持っていけない場合は50円の学校販売のパンを買う"
    ));
}

TEST_F(VoiceRecognitionTest, KoreanRecognition) {
    EXPECT_TRUE(TestRecognition(
        "test/test_data/ko.wav",
        "조 금만 생각 을 하 면서 살 면 훨씬 편할 거야"
    ));
}

TEST_F(VoiceRecognitionTest, CantoneseRecognition) {
    EXPECT_TRUE(TestRecognition(
        "test/test_data/yue.wav",
        "呢几个字都表达唔到我想讲嘅意思"
    ));
}

int main(int argc, char **argv) {
    // 设置工作目录环境变量
    if (!std::getenv("WORKSPACE_DIR")) {
        setenv("WORKSPACE_DIR", PROJECT_ROOT_DIR, 1);
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 