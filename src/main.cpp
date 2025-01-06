#include "api/http_server.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    try {
        // 默认配置
        std::string host = "0.0.0.0";
        int port = 8080;
        
        // 获取可执行文件所在目录
        fs::path exe_path = fs::absolute(argv[0]);
        fs::path workspace_path = exe_path.parent_path().parent_path();
        
        // 设置模型路径
        fs::path model_path = workspace_path / "models" / "sense-voice-small-fp16.gguf";
        
        // 从环境变量读取配置
        if (const char* env_port = std::getenv("SERVICE_PORT")) {
            port = std::stoi(env_port);
        }
        if (const char* env_model = std::getenv("MODEL_PATH")) {
            model_path = env_model;
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
        std::cout << "Using model: " << model_path << std::endl;

        // 运行服务器
        server.run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 