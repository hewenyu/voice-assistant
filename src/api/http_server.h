#pragma once

#include <memory>
#include <string>
#include <httplib.h>
#include "core/speech_recognizer.h"

namespace voice_assistant {

class HttpServer {
public:
    HttpServer(const std::string& host, int port);
    ~HttpServer();

    bool initialize(const std::string& model_path);
    void run();
    
    // 设置配置选项
    void set_api_key(const std::string& api_key) { api_key_ = api_key; }
    void set_max_request_size(size_t size) { max_request_size_ = size; }

private:
    void setup_routes();
    void handle_health_check(const httplib::Request& req, httplib::Response& res);
    void handle_recognize(const httplib::Request& req, httplib::Response& res);
    void handle_long_running_recognize(const httplib::Request& req, httplib::Response& res);
    bool parse_multipart_form_data(const httplib::Request& req, std::string& audio_data, RecognitionConfig& config);
    std::string generate_operation_id();
    std::string current_timestamp();
    
    // 认证相关
    bool verify_auth(const httplib::Request& req, httplib::Response& res);
    bool verify_api_key(const std::string& auth_header);
    
    // 请求大小验证
    bool verify_request_size(const httplib::Request& req, httplib::Response& res);

    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<SpeechRecognizer> recognizer_;
    std::string host_;
    int port_;
    std::string api_key_;  // API密钥
    size_t max_request_size_ = 10 * 1024 * 1024;  // 默认最大请求大小：10MB
};

} // namespace voice_assistant 