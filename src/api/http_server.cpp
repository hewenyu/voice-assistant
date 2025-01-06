#include "api/http_server.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>

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

    // 语音识别接口 - Google Speech-to-Text API 格式
    server_->Post("/v1/speech:recognize", [this](const httplib::Request& req, httplib::Response& res) {
        handle_recognize(req, res);
    });

    // 长语音识别接口
    server_->Post("/v1/speech:longrunningrecognize", [this](const httplib::Request& req, httplib::Response& res) {
        handle_long_running_recognize(req, res);
    });
}

void HttpServer::handle_health_check(const httplib::Request& req, httplib::Response& res) {
    json response = {
        {"status", "healthy"},
        {"timestamp", std::time(nullptr)}
    };
    res.set_content(response.dump(), "application/json");
}

void HttpServer::handle_recognize(const httplib::Request& req, httplib::Response& res) {
    try {
        // 检查Content-Type
        if (!req.has_header("Content-Type")) {
            throw std::runtime_error("Missing Content-Type header");
        }

        std::string content_type = req.get_header_value("Content-Type");
        json request_json;
        RecognitionConfig config;
        std::string audio_data;

        if (content_type == "application/json") {
            // 解析JSON请求
            request_json = json::parse(req.body);
            
            // 解析配置
            if (request_json.contains("config")) {
                auto& config_json = request_json["config"];
                if (config_json.contains("encoding")) {
                    config.encoding = config_json["encoding"].get<std::string>();
                }
                if (config_json.contains("sampleRateHertz")) {
                    config.sample_rate_hertz = config_json["sampleRateHertz"].get<int>();
                }
                if (config_json.contains("languageCode")) {
                    config.language_code = config_json["languageCode"].get<std::string>();
                }
                if (config_json.contains("enableAutomaticPunctuation")) {
                    config.enable_automatic_punctuation = config_json["enableAutomaticPunctuation"].get<bool>();
                }
            }

            // 获取音频数据
            if (!request_json.contains("audio") || !request_json["audio"].contains("content")) {
                throw std::runtime_error("Missing audio content in request");
            }
            
            // Base64解码音频数据
            audio_data = base64_decode(request_json["audio"]["content"].get<std::string>());
        } else if (content_type.find("multipart/form-data") != std::string::npos) {
            // 解析multipart form数据
            if (!parse_multipart_form_data(req, audio_data, config)) {
                throw std::runtime_error("Failed to parse multipart form data");
            }
        } else {
            throw std::runtime_error("Unsupported Content-Type: " + content_type);
        }

        // 创建临时文件保存音频数据
        char temp_path[] = "/tmp/voiceXXXXXX";
        int fd = mkstemp(temp_path);
        if (fd == -1) {
            throw std::runtime_error("Failed to create temporary file");
        }
        close(fd);

        {
            std::ofstream temp_file(temp_path, std::ios::binary);
            if (!temp_file) {
                std::remove(temp_path);
                throw std::runtime_error("Failed to write temporary file");
            }
            temp_file.write(audio_data.data(), audio_data.size());
        }

        // 执行识别
        auto result = recognizer_->recognize_sync(temp_path, config);

        // 删除临时文件
        std::remove(temp_path);

        // 构造Google Speech-to-Text API格式的响应
        json response;
        response["results"] = json::array();
        response["results"].push_back({
            {"alternatives", json::array({
                {
                    {"transcript", result.transcript},
                    {"confidence", result.confidence}
                }
            })},
            {"languageCode", config.language_code},
            {"resultEndTime", {
                {"seconds", static_cast<int64_t>(result.end_time)},
                {"nanos", static_cast<int32_t>((result.end_time - static_cast<int64_t>(result.end_time)) * 1e9)}
            }}
        });

        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        json error = {
            {"error", {
                {"code", 400},
                {"message", e.what()},
                {"status", "INVALID_ARGUMENT"}
            }}
        };
        res.status = 400;
        res.set_content(error.dump(), "application/json");
    }
}

void HttpServer::handle_long_running_recognize(const httplib::Request& req, httplib::Response& res) {
    try {
        // 解析请求和配置，与同步识别相同
        if (!req.has_header("Content-Type")) {
            throw std::runtime_error("Missing Content-Type header");
        }

        std::string content_type = req.get_header_value("Content-Type");
        json request_json;
        RecognitionConfig config;
        std::string audio_data;

        if (content_type == "application/json") {
            request_json = json::parse(req.body);
            // ... 解析配置和音频数据，与同步识别相同 ...
        }

        // 启动异步识别
        auto future = recognizer_->recognize_async(audio_data, config);

        // 生成操作ID
        std::string operation_id = generate_operation_id();

        // 返回长时间运行操作的响应
        json response = {
            {"name", "operations/" + operation_id},
            {"metadata", {
                {"@type", "type.googleapis.com/google.cloud.speech.v1.LongRunningRecognizeMetadata"},
                {"progressPercent", 0},
                {"startTime", current_timestamp()},
                {"lastUpdateTime", current_timestamp()}
            }},
            {"done", false}
        };

        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        json error = {
            {"error", {
                {"code", 400},
                {"message", e.what()},
                {"status", "INVALID_ARGUMENT"}
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
    
    if (!req.is_multipart_form_data()) {
        return false;
    }

    bool found_audio = false;
    bool found_config = false;

    const auto& files = req.files;
    for (const auto& file : files) {
        if (file.first == "audio") {
            // 读取音频文件内容
            const auto& content = file.second.content;
            audio_data = content;
            found_audio = true;
        } else if (file.first == "config") {
            try {
                // 读取配置内容
                const auto& content = file.second.content;
                auto config_json = json::parse(content);
                
                if (config_json.contains("encoding")) {
                    config.encoding = config_json["encoding"].get<std::string>();
                }
                if (config_json.contains("sampleRateHertz")) {
                    config.sample_rate_hertz = config_json["sampleRateHertz"].get<int>();
                }
                if (config_json.contains("languageCode")) {
                    config.language_code = config_json["languageCode"].get<std::string>();
                }
                if (config_json.contains("enableAutomaticPunctuation")) {
                    config.enable_automatic_punctuation = config_json["enableAutomaticPunctuation"].get<bool>();
                }
                found_config = true;
            } catch (const json::exception& e) {
                std::cerr << "Failed to parse config JSON: " << e.what() << std::endl;
                return false;
            }
        }
    }

    return found_audio;  // Config is optional
}

std::string HttpServer::base64_decode(const std::string& encoded) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string decoded;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r') continue;
        
        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) {
            throw std::runtime_error("Invalid base64 character");
        }
        
        val = (val << 6) + static_cast<int>(pos);
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

std::string HttpServer::generate_operation_id() {
    // 生成唯一的操作ID
    static int counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    return "voice-" + std::to_string(timestamp) + "-" + std::to_string(++counter);
}

std::string HttpServer::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    
    return std::to_string(timestamp);
}

} // namespace voice_assistant 