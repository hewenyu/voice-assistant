#include "api/http_server.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>

using json = nlohmann::json;

namespace voice_assistant {

// Base64 解码表
const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string HttpServer::base64_decode(const std::string& encoded) {
    std::string decoded;
    int val = 0, valb = -8;
    
    for (unsigned char c : encoded) {
        if (c == '=') break;
        
        // 在 base64_chars 中查找字符的位置
        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) continue;
        
        val = (val << 6) + static_cast<int>(pos);
        valb += 6;
        
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

HttpServer::HttpServer(const std::string& host, int port) 
    : server_(new httplib::Server()),
      recognizer_(new SpeechRecognizer()),
      host_(host),
      port_(port) {
    
    std::cout << "Server initializing on " << host_ << ":" << port_ << std::endl;
    setup_routes();
}

HttpServer::~HttpServer() = default;

bool HttpServer::initialize(const std::string& model_path) {
    try {
        if (!recognizer_->initialize(model_path)) {
            std::cerr << "Failed to initialize speech recognizer" << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        return false;
    }
}

void HttpServer::run() {
    std::cout << "Server starting on " << host_ << ":" << port_ << std::endl;
    
    // 设置服务器选项
    server_->set_keep_alive_max_count(1);
    server_->set_read_timeout(5);
    server_->set_write_timeout(5);
    server_->set_idle_interval(0, 100000);
    
    if (!server_->listen(host_.c_str(), port_)) {
        std::cerr << "Failed to start server on " << host_ << ":" << port_ << std::endl;
        return;
    }
    
    std::cout << "Server started successfully" << std::endl;
}

void HttpServer::setup_routes() {
    if (!server_) {
        std::cerr << "Server instance is null" << std::endl;
        return;
    }

    std::cout << "Setting up routes..." << std::endl;
    
    server_->Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handle_health_check(req, res);
    });
    
    server_->Post("/recognize", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            handle_recognize(req, res);
        } catch (const std::exception& e) {
            std::cerr << "Exception in recognize handler: " << e.what() << std::endl;
            json error = {
                {"error", {
                    {"code", 500},
                    {"message", std::string("Internal server error: ") + e.what()},
                    {"status", "INTERNAL"}
                }}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    server_->Post("/longrunningrecognize", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            handle_long_running_recognize(req, res);
        } catch (const std::exception& e) {
            std::cerr << "Exception in long running recognize handler: " << e.what() << std::endl;
            json error = {
                {"error", {
                    {"code", 500},
                    {"message", std::string("Internal server error: ") + e.what()},
                    {"status", "INTERNAL"}
                }}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    std::cout << "Routes setup completed" << std::endl;
}

void HttpServer::handle_health_check(const httplib::Request& req, httplib::Response& res) {
    json response = {
        {"status", "OK"},
        {"message", "Service is healthy"}
    };
    res.set_content(response.dump(), "application/json");
}

void HttpServer::handle_recognize(const httplib::Request& req, httplib::Response& res) {
    try {
        // 检查 Content-Type
        if (!req.has_header("Content-Type")) {
            throw std::runtime_error("Missing Content-Type header");
        }

        std::string content_type = req.get_header_value("Content-Type");
        RecognitionConfig config;

        if (content_type.find("multipart/form-data") != std::string::npos) {
            if (!parse_multipart_form_data(req, config)) {
                throw std::runtime_error("Failed to parse multipart form data");
            }
        } else if (content_type == "application/json") {
            json request_json = json::parse(req.body);
            
            // 解析配置
            if (request_json.contains("config")) {
                const auto& config_json = request_json["config"];
                config.encoding = config_json.value("encoding", "LINEAR16");
                config.sample_rate_hertz = config_json.value("sampleRateHertz", 16000);
                config.language_code = config_json.value("languageCode", "zh-CN");
                config.enable_automatic_punctuation = config_json.value("enableAutomaticPunctuation", true);
                config.max_alternatives = config_json.value("maxAlternatives", 1);
                config.profanity_filter = config_json.value("profanityFilter", false);
                config.enable_word_time_offsets = config_json.value("enableWordTimeOffsets", false);
                
                // 解析语音上下文
                if (config_json.contains("speechContexts")) {
                    for (const auto& context : config_json["speechContexts"]) {
                        RecognitionConfig::SpeechContext speech_context;
                        speech_context.phrases = context.value("phrases", std::vector<std::string>());
                        speech_context.boost = context.value("boost", 1.0f);
                        config.speech_contexts.push_back(speech_context);
                    }
                }
            }
            
            // 解析音频数据
            if (!request_json.contains("audio")) {
                throw std::runtime_error("Missing audio field in request");
            }
            
            const auto& audio_json = request_json["audio"];
            if (audio_json.contains("content")) {
                config.audio.content = audio_json["content"].get<std::string>();
            } else if (audio_json.contains("uri")) {
                config.audio.uri = audio_json["uri"].get<std::string>();
            } else {
                throw std::runtime_error("Either audio.content or audio.uri must be provided");
            }
        } else {
            throw std::runtime_error("Unsupported Content-Type");
        }

        // 处理音频数据
        std::string audio_data;
        if (!config.audio.content.empty()) {
            // 从 base64 内容解码
            audio_data = base64_decode(config.audio.content);
        } else if (!config.audio.uri.empty()) {
            // TODO: 从 URI 获取音频数据
            throw std::runtime_error("Audio URI support not implemented yet");
        } else {
            throw std::runtime_error("No audio data provided");
        }

        // 创建临时文件存储音频数据
        std::string temp_path = "/tmp/audio_" + generate_operation_id() + ".wav";
        std::ofstream temp_file(temp_path, std::ios::binary);
        if (!temp_file) {
            throw std::runtime_error("Failed to create temporary file");
        }
        temp_file.write(audio_data.c_str(), audio_data.size());
        temp_file.close();

        // 执行识别
        auto result = recognizer_->recognize_sync(temp_path, config);

        // 删除临时文件
        std::remove(temp_path.c_str());

        // 构造响应
        json response;
        response["results"] = json::array();
        
        json result_json;
        result_json["alternatives"] = json::array();
        
        json alternative;
        alternative["transcript"] = result.transcript;
        alternative["confidence"] = result.confidence;
        
        // 添加词时间戳
        if (config.enable_word_time_offsets && !result.words.empty()) {
            alternative["words"] = json::array();
            for (const auto& word : result.words) {
                alternative["words"].push_back({
                    {"word", word.word},
                    {"startTime", {
                        {"seconds", static_cast<int64_t>(word.start_time)},
                        {"nanos", static_cast<int32_t>((word.start_time - static_cast<int64_t>(word.start_time)) * 1e9)}
                    }},
                    {"endTime", {
                        {"seconds", static_cast<int64_t>(word.end_time)},
                        {"nanos", static_cast<int32_t>((word.end_time - static_cast<int64_t>(word.end_time)) * 1e9)}
                    }}
                });
            }
        }
        
        result_json["alternatives"].push_back(alternative);
        result_json["languageCode"] = config.language_code;
        
        if (result.is_final) {
            result_json["isFinal"] = true;
        }
        
        response["results"].push_back(result_json);
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
    // 生成操作ID
    std::string operation_id = generate_operation_id();
    
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
}

bool HttpServer::parse_multipart_form_data(
    const httplib::Request& req,
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
            config.audio.content = base64_decode(file.second.content);
            found_audio = true;
        } else if (file.first == "config") {
            try {
                // 读取配置内容
                auto config_json = json::parse(file.second.content);
                
                config.encoding = config_json.value("encoding", "LINEAR16");
                config.sample_rate_hertz = config_json.value("sampleRateHertz", 16000);
                config.language_code = config_json.value("languageCode", "zh-CN");
                config.enable_automatic_punctuation = config_json.value("enableAutomaticPunctuation", true);
                config.max_alternatives = config_json.value("maxAlternatives", 1);
                config.profanity_filter = config_json.value("profanityFilter", false);
                config.enable_word_time_offsets = config_json.value("enableWordTimeOffsets", false);
                
                // 解析语音上下文
                if (config_json.contains("speechContexts")) {
                    for (const auto& context : config_json["speechContexts"]) {
                        RecognitionConfig::SpeechContext speech_context;
                        speech_context.phrases = context.value("phrases", std::vector<std::string>());
                        speech_context.boost = context.value("boost", 1.0f);
                        config.speech_contexts.push_back(speech_context);
                    }
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

std::string HttpServer::generate_operation_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex_digits = "0123456789abcdef";

    std::string uuid;
    uuid.reserve(36);

    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else {
            uuid += hex_digits[dis(gen)];
        }
    }

    return uuid;
}

std::string HttpServer::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count() % 1000;

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&now_c), "%Y-%m-%dT%H:%M:%S") 
       << '.' << std::setfill('0') << std::setw(3) << now_ms << 'Z';

    return ss.str();
}

} // namespace voice_assistant 