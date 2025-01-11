#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include "core/voice_service_impl.h"

// Helper function to check if file exists
bool file_exists(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

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
              << "\nVAD Options:\n"
              << "  -v, --vad-model PATH      Path to VAD model file (required)\n"
              << "  --vad-threshold FLOAT     VAD threshold (default: 0.5)\n"
              << "  --vad-min-silence FLOAT   Min silence duration in seconds (default: 0.5)\n"
              << "  --vad-min-speech FLOAT    Min speech duration in seconds (default: 0.25)\n"
              << "  --vad-max-speech FLOAT    Max speech duration in seconds (default: 5.0)\n"
              << "  --vad-window-size INT     Window size in samples (default: 512)\n"
              << "  --sample-rate INT         Audio sample rate (default: 16000)\n"
              << "  -h, --help                Show this help message\n";
}

ModelConfig parse_arguments(int argc, char** argv) {
    ModelConfig config;
    config.num_threads = 4;  // default value
    config.provider = "cpu";
    config.language = "auto";
    std::string port = "50051";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        }
        // Basic model options
        else if ((arg == "--model-path" || arg == "-m") && i + 1 < argc) {
            config.model_path = argv[++i];
        }
        else if ((arg == "--tokens-path" || arg == "-t") && i + 1 < argc) {
            config.tokens_path = argv[++i];
        }
        else if ((arg == "--language" || arg == "-l") && i + 1 < argc) {
            config.language = argv[++i];
        }
        else if ((arg == "--num-threads" || arg == "-n") && i + 1 < argc) {
            config.num_threads = std::stoi(argv[++i]);
        }
        else if ((arg == "--provider" || arg == "-p") && i + 1 < argc) {
            config.provider = argv[++i];
        }
        else if (arg == "--debug" || arg == "-d") {
            config.debug = true;
        }
        else if ((arg == "--port" || arg == "-P") && i + 1 < argc) {
            port = argv[++i];
        }
        // VAD options
        else if ((arg == "--vad-model" || arg == "-v") && i + 1 < argc) {
            config.vad_model_path = argv[++i];
        }
        else if (arg == "--vad-threshold" && i + 1 < argc) {
            config.vad_threshold = std::stof(argv[++i]);
        }
        else if (arg == "--vad-min-silence" && i + 1 < argc) {
            config.vad_min_silence_duration = std::stof(argv[++i]);
        }
        else if (arg == "--vad-min-speech" && i + 1 < argc) {
            config.vad_min_speech_duration = std::stof(argv[++i]);
        }
        else if (arg == "--vad-max-speech" && i + 1 < argc) {
            config.vad_max_speech_duration = std::stof(argv[++i]);
        }
        else if (arg == "--vad-window-size" && i + 1 < argc) {
            config.vad_window_size = std::stoi(argv[++i]);
        }
        else if (arg == "--sample-rate" && i + 1 < argc) {
            config.sample_rate = std::stoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }

    // Set default values for optional parameters
    config.set_defaults();

    // Validate configuration
    if (!config.validate()) {
        std::cerr << "Error in configuration:\n" << config.get_error_message();
        print_usage(argv[0]);
        exit(1);
    }

    // Validate file existence
    if (!file_exists(config.model_path)) {
        std::cerr << "Error: Model file not found: " << config.model_path << "\n";
        exit(1);
    }
    if (!file_exists(config.tokens_path)) {
        std::cerr << "Error: Tokens file not found: " << config.tokens_path << "\n";
        exit(1);
    }
    if (!file_exists(config.vad_model_path)) {
        std::cerr << "Error: VAD model file not found: " << config.vad_model_path << "\n";
        exit(1);
    }

    return config;
}

int main(int argc, char** argv) {
    try {
        std::cout << "Starting voice service..." << std::endl;
        
        // Parse command line arguments
        ModelConfig config = parse_arguments(argc, argv);
        
        std::cout << "Configuration loaded:" << std::endl
                  << "  Model path: " << config.model_path << std::endl
                  << "  Tokens path: " << config.tokens_path << std::endl
                  << "  VAD model path: " << config.vad_model_path << std::endl
                  << "  Language: " << config.language << std::endl
                  << "  Provider: " << config.provider << std::endl
                  << "  Threads: " << config.num_threads << std::endl;

        // Initialize service with configuration
        std::string server_address("0.0.0.0:50051");
        std::cout << "Initializing voice service..." << std::endl;
        VoiceServiceImpl service(config);

        std::cout << "Setting up gRPC server..." << std::endl;
        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::cout << "Starting server..." << std::endl;
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;

        server->Wait();
    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }

    return 0;
} 