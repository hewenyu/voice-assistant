#include "core/model_config.h"
#include "core/voice_service_impl.h"
#include <iostream>
#include <string>

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " CONFIG_FILE" << std::endl;
    std::cerr << "Example: " << program << " ../config/config.yaml" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        // Load configuration from file
        std::string config_path = argv[1];
        ModelConfig config = ModelConfig::LoadFromFile(config_path);
        
        std::cout << "Configuration loaded successfully" << std::endl;
        
        // Initialize voice service
        VoiceServiceImpl service(config);
        std::cout << "Voice service initialized successfully" << std::endl;
        
        // TODO: Add your client implementation here
        // For example:
        // - Set up gRPC server
        // - Handle voice input/output
        // - Implement user interface
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 