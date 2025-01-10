#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "core/voice_service_impl.h"
#include <filesystem>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -m, --model-path PATH     Path to the model file (required)\n"
              << "  -t, --tokens-path PATH    Path to the tokens file (required)\n"
              << "  -l, --language LANG       Language code (default: auto)\n"
              << "  -n, --num-threads N       Number of threads (default: 4)\n"
              << "  -p, --provider TYPE       Provider type (default: cpu)\n"
              << "  -d, --debug               Enable debug mode\n"
              << "  -P, --port PORT           Server port (default: 50051)\n"
              << "  -h, --help                Show this help message\n";
}

ModelConfig parse_arguments(int argc, char** argv) {
    ModelConfig config;
    std::string port = "50051";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // Help
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        }
        // Model path
        else if ((arg == "--model-path" || arg == "-m") && i + 1 < argc) {
            config.model_path = argv[++i];
        }
        // Tokens path
        else if ((arg == "--tokens-path" || arg == "-t") && i + 1 < argc) {
            config.tokens_path = argv[++i];
        }
        // Language
        else if ((arg == "--language" || arg == "-l") && i + 1 < argc) {
            config.language = argv[++i];
        }
        // Number of threads
        else if ((arg == "--num-threads" || arg == "-n") && i + 1 < argc) {
            config.num_threads = std::stoi(argv[++i]);
        }
        // Provider
        else if ((arg == "--provider" || arg == "-p") && i + 1 < argc) {
            config.provider = argv[++i];
        }
        // Debug mode
        else if (arg == "--debug" || arg == "-d") {
            config.debug = true;
        }
        // Port
        else if ((arg == "--port" || arg == "-P") && i + 1 < argc) {
            port = argv[++i];
        }
        // Unknown argument
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }

    // Validate required parameters
    if (config.model_path.empty() || config.tokens_path.empty()) {
        std::cerr << "Error: model-path and tokens-path are required\n";
        print_usage(argv[0]);
        exit(1);
    }

    // Validate file existence
    if (!std::filesystem::exists(config.model_path)) {
        std::cerr << "Error: Model file not found: " << config.model_path << "\n";
        exit(1);
    }
    if (!std::filesystem::exists(config.tokens_path)) {
        std::cerr << "Error: Tokens file not found: " << config.tokens_path << "\n";
        exit(1);
    }

    return config;
}

int main(int argc, char** argv) {
    // Parse command line arguments
    ModelConfig config = parse_arguments(argc, argv);

    // Initialize service with configuration
    std::string server_address("0.0.0.0:50051");
    VoiceServiceImpl service(config);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();

    return 0;
} 