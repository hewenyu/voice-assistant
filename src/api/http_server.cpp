#include "api/http_server.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace voice_assistant {

HttpServer::HttpServer(const std::string& host, int port) {
    server_ = std::make_unique<httplib::Server>();
    server_->bind_to_port(host.c_str(), port);
}

HttpServer::~HttpServer() = default;

bool HttpServer::initialize(const std::string& model_path) {
    recognizer_ = std::make_unique<SpeechRecognizer>();
    if (!recognizer_->initialize(model_path)) {
        return false;
    }
    setup_routes();
    return true;
}

void HttpServer::run() {
    server_->listen_after_bind();
}

void HttpServer::setup_routes() {
    // 健康检查接口
    server_->Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handle_health_check(req, res);
    });

    // 语音识别接口
    server_->Post("/v1/speech:recognize", [this](const httplib::Request& req, httplib::Response& res) {
        handle_recognize(req, res);
    });
}

void HttpServer::handle_health_check(const httplib::Request& req, httplib::Response& res) {
    json response = {
        {"status", "healthy"}
    };
    res.set_content(response.dump(), "application/json");
}

void HttpServer::handle_recognize(const httplib::Request& req, httplib::Response& res) {
    try {
        // 检查Content-Type
        if (!req.has_header("Content-Type")) {
            throw std::runtime_error("Missing Content-Type header");
        }

        std::string audio_data;
        RecognitionConfig config;

        // 解析multipart form数据
        if (!parse_multipart_form_data(req, audio_data, config)) {
            throw std::runtime_error("Failed to parse multipart form data");
        }

        // 创建临时文件保存音频数据
        std::string temp_path = std::tmpnam(nullptr);
        {
            std::ofstream temp_file(temp_path, std::ios::binary);
            temp_file.write(audio_data.data(), audio_data.size());
        }

        // 执行识别
        auto result = recognizer_->recognize_file(temp_path, config);

        // 删除临时文件
        std::remove(temp_path.c_str());

        // 构造响应
        json response = {
            {"results", {
                {
                    {"alternatives", {
                        {
                            {"transcript", result.transcript},
                            {"confidence", result.confidence}
                        }
                    }}
                }
            }}
        };

        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        json error = {
            {"error", {
                {"message", e.what()},
                {"code", 400}
            }}
        };
        res.status = 400;
        res.set_content(error.dump(), "application/json");
    }
}

bool HttpServer::parse_multipart_form_data(
    const httplib::Request& req,
    std::string& audio_data,
    RecognitionConfig& config) {
    
    // TODO: 实现multipart form-data解析
    // 1. 解析音频数据
    // 2. 解析配置参数
    
    return true;
}

} // namespace voice_assistant 