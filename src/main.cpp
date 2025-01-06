#include "api/http_server.h"
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    try {
        // 默认配置
        std::string host = "0.0.0.0";
        int port = 8080;
        std::string model_path = "../models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.onnx";

        // 从环境变量读取配置
        if (const char* env_port = std::getenv("SERVICE_PORT")) {
            port = std::stoi(env_port);
        }
        if (const char* env_model = std::getenv("MODEL_PATH")) {
            model_path = env_model;
        }

        // 创建并初始化服务器
        voice_assistant::HttpServer server(host, port);
        
        if (!server.initialize(model_path)) {
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