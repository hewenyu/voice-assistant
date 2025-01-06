#include "api/http_server.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -m, --model FNAME    Model path (default: models/sense-voice-small-fp16.gguf)\n"
              << "  -h, --help           Show this help message\n"
              << "  -p, --port PORT      Server port (default: 8080)\n";
}

int main(int argc, char* argv[]) {
    try {
        // 默认配置
        std::string host = "0.0.0.0";
        int port = 8080;
        
        // 获取当前工作目录
        fs::path current_path = fs::current_path();
        
        // 设置默认模型路径
        fs::path model_path = current_path / "models" / "sense-voice-small-fp16.gguf";

        // 解析命令行参数
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            } else if (arg == "-m" || arg == "--model") {
                if (i + 1 < argc) {
                    model_path = argv[++i];
                } else {
                    std::cerr << "Error: Model path argument missing\n";
                    print_usage(argv[0]);
                    return 1;
                }
            } else if (arg == "-p" || arg == "--port") {
                if (i + 1 < argc) {
                    port = std::stoi(argv[++i]);
                } else {
                    std::cerr << "Error: Port argument missing\n";
                    print_usage(argv[0]);
                    return 1;
                }
            }
        }
        
        // 从环境变量读取配置（环境变量优先级低于命令行参数）
        if (const char* env_port = std::getenv("SERVICE_PORT")) {
            if (port == 8080) { // 只有在命令行没有指定时才使用环境变量
                port = std::stoi(env_port);
            }
        }
        if (const char* env_model = std::getenv("MODEL_PATH")) {
            if (model_path == current_path / "models" / "sense-voice-small-fp16.gguf") {
                model_path = env_model;
            }
        }

        // 检查模型文件是否存在
        if (!fs::exists(model_path)) {
            std::cerr << "Error: Model file not found: " << model_path << std::endl;
            return 1;
        }

        std::cout << "Using model: " << model_path << std::endl;

        // 创建并初始化服务器
        voice_assistant::HttpServer server(host, port);
        
        if (!server.initialize(model_path.string())) {
            std::cerr << "Failed to initialize server" << std::endl;
            return 1;
        }

        std::cout << "Server starting on " << host << ":" << port << std::endl;

        // 运行服务器
        server.run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 