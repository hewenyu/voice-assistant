#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <codecvt>
#include <locale>

using voice::VoiceService;
using voice::SyncRecognizeRequest;
using voice::SyncRecognizeResponse;
using voice::RecognitionConfig;
using voice::AudioEncoding;

class VoiceRecognitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        channel_ = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
        stub_ = VoiceService::NewStub(channel_);

        std::cout << "Waiting for server to be ready..." << std::endl;
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        bool connected = channel_->WaitForConnected(deadline);
        ASSERT_TRUE(connected) << "Failed to connect to server. Is the server running?";
        std::cout << "Server is ready." << std::endl;
    }

    void PrintMessage(const google::protobuf::Message& message, const std::string& name) {
        std::string json_string;
        google::protobuf::util::JsonPrintOptions options;
        options.add_whitespace = true;
        options.always_print_primitive_fields = true;
        google::protobuf::util::MessageToJsonString(message, &json_string, options);
        std::cout << name << ":\n" << json_string << std::endl;
    }

    std::string ReadFile(const char* filename) {
        const char* workspace_dir = std::getenv("WORKSPACE_DIR");
        if (!workspace_dir) {
            ADD_FAILURE() << "WORKSPACE_DIR environment variable not set";
            return "";
        }

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

    bool TestRecognition(const char* filename) {
        std::string audio_data = ReadFile(filename);
        if (audio_data.empty()) {
            return false;
        }

        std::cout << "\n=== Testing file: " << filename << " ===" << std::endl;
        std::cout << "Audio data size: " << audio_data.size() << " bytes" << std::endl;

        SyncRecognizeRequest request;
        auto* config = request.mutable_config();
        config->set_encoding(AudioEncoding::LINEAR16);
        config->set_sample_rate_hertz(16000);
        config->set_language_code("auto");
        request.set_audio_content(audio_data);

        SyncRecognizeResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(30));
        context.AddMetadata("x-request-id", "test-" + std::string(filename));

        grpc::Status status = stub_->SyncRecognize(&context, request, &response);

        std::cout << "\nResponse status:" << std::endl;
        std::cout << "  OK: " << status.ok() << std::endl;
        if (!status.ok()) {
            std::cout << "  Error code: " << status.error_code() << std::endl;
            std::cout << "  Error message: " << status.error_message() << std::endl;
            return false;
        }
        
        if (response.results_size() > 0) {
            std::cout << "\nRecognition results:" << std::endl;
            for (int i = 0; i < response.results_size(); i++) {
                const auto& result = response.results(i);
                std::cout << "Result " << i + 1 << ":" << std::endl;
                
                for (int j = 0; j < result.alternatives_size(); j++) {
                    const auto& alternative = result.alternatives(j);
                    std::cout << "  Alternative " << j + 1 << ":" << std::endl;
                    std::cout << "    Transcript: " << alternative.transcript() << std::endl;
                    std::cout << "    Confidence: " << alternative.confidence() << std::endl;

                    if (alternative.words_size() > 0) {
                        std::cout << "    Words with timing:" << std::endl;
                        for (const auto& word : alternative.words()) {
                            double start_time = word.start_time().seconds() + 
                                              word.start_time().nanos() / 1e9;
                            double end_time = word.end_time().seconds() + 
                                            word.end_time().nanos() / 1e9;
                            
                            std::cout << "      " << word.word()
                                     << " [" << std::fixed << std::setprecision(3)
                                     << start_time << "s -> " 
                                     << end_time << "s]" << std::endl;
                        }
                    }
                }
            }
            return true;
        }

        std::cout << "No recognition results received" << std::endl;
        return false;
    }

    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<VoiceService::Stub> stub_;
};

TEST_F(VoiceRecognitionTest, EnglishRecognition) {
    TestRecognition("test/test_data/en.wav");
}

TEST_F(VoiceRecognitionTest, ChineseRecognition) {
    TestRecognition("test/test_data/zh.wav");
}

TEST_F(VoiceRecognitionTest, JapaneseRecognition) {
    TestRecognition("test/test_data/ja.wav");
}

TEST_F(VoiceRecognitionTest, KoreanRecognition) {
    TestRecognition("test/test_data/ko.wav");
}

TEST_F(VoiceRecognitionTest, CantoneseRecognition) {
    TestRecognition("test/test_data/yue.wav");
}

int main(int argc, char **argv) {
    if (!std::getenv("WORKSPACE_DIR")) {
        setenv("WORKSPACE_DIR", ".", 1);
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 