#pragma once

#include <memory>
#include <string>
#include "httplib.h"
#include "core/speech_recognizer.h"

namespace voice_assistant {

class HttpServer {
public:
    HttpServer(const std::string& host, int port);
    ~HttpServer();

    bool initialize(const std::string& model_path);
    void run();

private:
    void setup_routes();
    void handle_health_check(const httplib::Request& req, httplib::Response& res);
    void handle_recognize(const httplib::Request& req, httplib::Response& res);
    void handle_long_running_recognize(const httplib::Request& req, httplib::Response& res);
    
    bool parse_multipart_form_data(
        const httplib::Request& req,
        std::string& audio_data,
        RecognitionConfig& config);
    
    std::string base64_decode(const std::string& encoded);
    std::string generate_operation_id();
    std::string current_timestamp();

    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<SpeechRecognizer> recognizer_;
};

} // namespace voice_assistant 