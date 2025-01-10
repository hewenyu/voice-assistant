#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"

std::string ReadFile(const char* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer;
    buffer.resize(size);
    if (!file.read(buffer.data(), size)) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return "";
    }

    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <audio_file>" << std::endl;
        return 1;
    }

    // Create channel
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    std::unique_ptr<VoiceService::Stub> stub = VoiceService::NewStub(channel);

    // Read audio file
    std::string audio_data = ReadFile(argv[1]);
    if (audio_data.empty()) {
        return 1;
    }

    // Create request
    SyncRecognizeRequest request;
    request.set_audio_data(audio_data);

    // Call RPC
    SyncRecognizeResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub->SyncRecognize(&context, request, &response);

    if (!status.ok()) {
        std::cerr << "RPC failed: " << status.error_message() << std::endl;
        return 1;
    }

    // Print result
    std::cout << "Recognition result: " << response.text() << std::endl;

    return 0;
} 