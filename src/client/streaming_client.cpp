#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include <thread>
#include <chrono>

// Preprocess audio file using sox
std::string PreprocessAudio(const std::string& input_file) {
    std::string output_file = input_file + ".16k.wav";
    std::string cmd = "sox \"" + input_file + "\" -r 16000 -c 1 -b 16 \"" + output_file + "\"";
    
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Failed to preprocess audio file" << std::endl;
        return "";
    }
    
    return output_file;
}

// 读取音频文件
std::string ReadFile(const char* filename) {
    // First preprocess the audio file
    std::string processed_file = PreprocessAudio(filename);
    if (processed_file.empty()) {
        return "";
    }

    std::ifstream file(processed_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << processed_file << std::endl;
        return "";
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer;
    buffer.resize(size);
    if (!file.read(const_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read file: " << processed_file << std::endl;
        std::remove(processed_file.c_str());  // Clean up temporary file
        return "";
    }

    std::remove(processed_file.c_str());  // Clean up temporary file
    return buffer;
}

// 模拟实时音频流
class StreamingClient {
public:
    StreamingClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(VoiceService::NewStub(channel)) {}

    void StreamingRecognize(const std::string& audio_data) {
        grpc::ClientContext context;
        std::shared_ptr<grpc::ClientReaderWriter<StreamingRecognizeRequest, 
                                                StreamingRecognizeResponse>> stream(
            stub_->StreamingRecognize(&context));

        // 启动读取响应的线程
        std::thread reader([stream]() {
            StreamingRecognizeResponse response;
            while (stream->Read(&response)) {
                std::cout << "Recognition " 
                         << (response.is_final() ? "(final): " : "(interim): ")
                         << response.text() << std::endl;
            }
        });

        // 模拟实时发送音频数据
        // 每次发送1s的音频 (16kHz * 2bytes * 1s)
        const size_t chunk_size = 32000; 
        for (size_t i = 0; i < audio_data.size(); i += chunk_size) {
            StreamingRecognizeRequest request;
            size_t current_chunk_size = std::min(chunk_size, audio_data.size() - i);
            request.set_audio_data(audio_data.substr(i, current_chunk_size));
            
            if (!stream->Write(request)) {
                break;
            }

            // 模拟实时性，每1s发送一次数据
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        stream->WritesDone();
        reader.join();
    }

private:
    std::unique_ptr<VoiceService::Stub> stub_;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <audio_file>" << std::endl;
        return 1;
    }

    // 创建channel
    auto channel = grpc::CreateChannel("localhost:50051", 
                                     grpc::InsecureChannelCredentials());
    StreamingClient client(channel);

    // 读取音频文件
    std::string audio_data = ReadFile(argv[1]);
    if (audio_data.empty()) {
        return 1;
    }

    std::cout << "Starting streaming recognition..." << std::endl;
    client.StreamingRecognize(audio_data);
    std::cout << "Streaming recognition completed." << std::endl;

    return 0;
} 