#pragma once

#include <memory>
#include <string>
#include "httplib.h"
#include "core/speech_recognizer.h"

namespace voice_assistant {

class HttpServer {
public:
    HttpServer(const std::string& host = "0.0.0.0", int port = 8080);
    ~HttpServer();

    // 初始化服务器
    bool initialize(const std::string& model_path);

    // 启动服务器
    void run();

private:
    // HTTP服务器实例
    std::unique_ptr<httplib::Server> server_;
    // 语音识别器实例
    std::unique_ptr<SpeechRecognizer> recognizer_;

    // API路由处理函数
    void setup_routes();
    void handle_recognize(const httplib::Request& req, httplib::Response& res);
    void handle_health_check(const httplib::Request& req, httplib::Response& res);

    // 工具函数
    bool parse_multipart_form_data(const httplib::Request& req, 
                                 std::string& audio_data,
                                 RecognitionConfig& config);
};

} // namespace voice_assistant 