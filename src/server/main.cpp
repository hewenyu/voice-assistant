#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "core/voice_service_impl.h"
#include "core/model_config.h"

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program << " <config_file>" << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    try {
        // Load configuration from file
        std::string config_file = argv[1];
        ModelConfig config = ModelConfig::LoadFromFile(config_file);
        
        // Create server
        std::string server_address("0.0.0.0:50051");
        VoiceServiceImpl service(config);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;

        server->Wait();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 