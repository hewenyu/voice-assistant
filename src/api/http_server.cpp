#include "api/http_server.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <regex>

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

bool HttpServer::verify_auth(const httplib::Request& req, httplib::Response& res) {
    // 检查是否需要认证
    if (api_key_.empty()) {
        return true;  // 如果未设置API密钥，则不需要认证
    }

    // 检查Authorization头
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        json error = {
            {"error", {
                {"code", 401},
                {"message", "Missing Authorization header"},
                {"status", "UNAUTHENTICATED"}
            }}
        };
        res.status = 401;
        res.set_content(error.dump(), "application/json");
        return false;
    }

    // 验证API密钥
    if (!verify_api_key(auth_header)) {
        json error = {
            {"error", {
                {"code", 401},
                {"message", "Invalid API key"},
                {"status", "UNAUTHENTICATED"}
            }}
        };
        res.status = 401;
        res.set_content(error.dump(), "application/json");
        return false;
    }

    return true;
}

bool HttpServer::verify_api_key(const std::string& auth_header) {
    // 检查Bearer token格式
    std::regex bearer_regex("Bearer\\s+(.+)");
    std::smatch matches;
    if (!std::regex_match(auth_header, matches, bearer_regex)) {
        return false;
    }

    // 提取并验证token
    std::string token = matches[1].str();
    return token == api_key_;
}

bool HttpServer::verify_request_size(const httplib::Request& req, httplib::Response& res) {
    size_t content_length = 0;
    if (auto content_length_str = req.get_header_value("Content-Length"); !content_length_str.empty()) {
        content_length = std::stoull(content_length_str);
    }

    if (content_length > max_request_size_) {
        json error = {
            {"error", {
                {"code", 413},
                {"message", "Request entity too large"},
                {"status", "FAILED_PRECONDITION"}
            }}
        };
        res.status = 413;
        res.set_content(error.dump(), "application/json");
        return false;
    }

    return true;
}

void HttpServer::handle_health_check(const httplib::Request& req, httplib::Response& res) {
    // 验证认证
    if (!verify_auth(req, res)) {
        return;
    }

    json response = {
        {"status", "OK"},
        {"message", "Service is healthy"}
    };
    res.set_content(response.dump(), "application/json");
}

void HttpServer::handle_recognize(const httplib::Request& req, httplib::Response& res) {
    try {
        std::cout << "Received recognize request" << std::endl;
        
        // 验证认证
        if (!verify_auth(req, res)) {
            std::cerr << "Authentication failed" << std::endl;
            return;
        }

        // 验证请求大小
        if (!verify_request_size(req, res)) {
            std::cerr << "Request size validation failed" << std::endl;
            return;
        }

        // 检查 Content-Type
        if (!req.has_header("Content-Type")) {
            std::cerr << "Missing Content-Type header" << std::endl;
            throw std::runtime_error("Missing Content-Type header");
        }

        std::string content_type = req.get_header_value("Content-Type");
        std::cout << "Content-Type: " << content_type << std::endl;
        
        RecognitionConfig config;
        std::string audio_data;

        try {
            if (content_type.find("multipart/form-data") != std::string::npos) {
                std::cout << "Processing multipart form data" << std::endl;
                if (!parse_multipart_form_data(req, config)) {
                    throw std::runtime_error("Failed to parse multipart form data");
                }
            } else if (content_type == "application/json") {
                std::cout << "Processing JSON request" << std::endl;
                json request_json = json::parse(req.body);
                std::cout << "Request body parsed successfully" << std::endl;
                
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
                    
                    std::cout << "Config parsed: " << std::endl
                             << "- encoding: " << config.encoding << std::endl
                             << "- sample_rate: " << config.sample_rate_hertz << std::endl
                             << "- language: " << config.language_code << std::endl;
                }
                
                // 解析音频数据
                if (!request_json.contains("audio")) {
                    std::cerr << "Missing audio field in request" << std::endl;
                    throw std::runtime_error("Missing audio field in request");
                }
                
                const auto& audio_json = request_json["audio"];
                if (audio_json.contains("content")) {
                    config.audio.content = audio_json["content"].get<std::string>();
                    std::cout << "Audio content length: " << config.audio.content.length() << std::endl;
                } else if (audio_json.contains("uri")) {
                    config.audio.uri = audio_json["uri"].get<std::string>();
                    std::cout << "Audio URI: " << config.audio.uri << std::endl;
                } else {
                    std::cerr << "Either audio.content or audio.uri must be provided" << std::endl;
                    throw std::runtime_error("Either audio.content or audio.uri must be provided");
                }
            } else {
                std::cerr << "Unsupported Content-Type: " << content_type << std::endl;
                throw std::runtime_error("Unsupported Content-Type");
            }

            // 处理音频数据
            if (!config.audio.content.empty()) {
                std::cout << "Decoding base64 audio content" << std::endl;
                std::string decoded_data = base64_decode(config.audio.content);
                std::cout << "Decoded audio size: " << decoded_data.size() << " bytes" << std::endl;

                // 检查音频数据是否已经是WAV格式
                bool is_wav = false;
                if (decoded_data.size() > 44) {  // WAV头部至少44字节
                    is_wav = (decoded_data.substr(0, 4) == "RIFF" && 
                             decoded_data.substr(8, 4) == "WAVE");
                    
                    // 打印WAV头部信息
                    if (is_wav) {
                        std::cout << "Input is WAV format, analyzing header:" << std::endl;
                        
                        // 读取文件大小
                        uint32_t file_size;
                        std::memcpy(&file_size, decoded_data.data() + 4, 4);
                        std::cout << "- File size: " << file_size + 8 << " bytes" << std::endl;
                        
                        // 读取格式块
                        uint32_t fmt_chunk_size;
                        std::memcpy(&fmt_chunk_size, decoded_data.data() + 16, 4);
                        std::cout << "- Format chunk size: " << fmt_chunk_size << " bytes" << std::endl;
                        
                        uint16_t audio_format;
                        std::memcpy(&audio_format, decoded_data.data() + 20, 2);
                        std::cout << "- Audio format: " << audio_format << " (1 = PCM)" << std::endl;
                        
                        uint16_t num_channels;
                        std::memcpy(&num_channels, decoded_data.data() + 22, 2);
                        std::cout << "- Number of channels: " << num_channels << std::endl;
                        
                        uint32_t sample_rate;
                        std::memcpy(&sample_rate, decoded_data.data() + 24, 4);
                        std::cout << "- Sample rate: " << sample_rate << " Hz" << std::endl;
                        
                        uint32_t byte_rate;
                        std::memcpy(&byte_rate, decoded_data.data() + 28, 4);
                        std::cout << "- Byte rate: " << byte_rate << " bytes/sec" << std::endl;
                        
                        uint16_t block_align;
                        std::memcpy(&block_align, decoded_data.data() + 32, 2);
                        std::cout << "- Block align: " << block_align << " bytes" << std::endl;
                        
                        uint16_t bits_per_sample;
                        std::memcpy(&bits_per_sample, decoded_data.data() + 34, 2);
                        std::cout << "- Bits per sample: " << bits_per_sample << std::endl;
                        
                        // 查找数据块
                        size_t data_pos = decoded_data.find("data", 36);
                        if (data_pos != std::string::npos) {
                            uint32_t data_size;
                            std::memcpy(&data_size, decoded_data.data() + data_pos + 4, 4);
                            std::cout << "- Data chunk size: " << data_size << " bytes" << std::endl;
                        }
                    }
                }

                if (is_wav) {
                    std::cout << "Input is already in WAV format" << std::endl;
                    audio_data = decoded_data;
                } else {
                    std::cout << "Converting raw PCM to WAV format" << std::endl;
                    // 构造WAV头部
                    std::stringstream wav_header;
                    uint32_t data_size = decoded_data.size();
                    uint32_t file_size = data_size + 44 - 8;  // Total file size - 8 bytes

                    // RIFF header
                    wav_header.write("RIFF", 4);
                    wav_header.write(reinterpret_cast<const char*>(&file_size), 4);
                    wav_header.write("WAVE", 4);

                    // fmt chunk
                    wav_header.write("fmt ", 4);
                    uint32_t fmt_chunk_size = 16;
                    uint16_t audio_format = 1;  // PCM
                    uint16_t num_channels = 1;  // Mono
                    uint32_t sample_rate = config.sample_rate_hertz;
                    uint16_t bits_per_sample = 16;
                    uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
                    uint16_t block_align = num_channels * bits_per_sample / 8;

                    wav_header.write(reinterpret_cast<const char*>(&fmt_chunk_size), 4);
                    wav_header.write(reinterpret_cast<const char*>(&audio_format), 2);
                    wav_header.write(reinterpret_cast<const char*>(&num_channels), 2);
                    wav_header.write(reinterpret_cast<const char*>(&sample_rate), 4);
                    wav_header.write(reinterpret_cast<const char*>(&byte_rate), 4);
                    wav_header.write(reinterpret_cast<const char*>(&block_align), 2);
                    wav_header.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

                    // data chunk
                    wav_header.write("data", 4);
                    wav_header.write(reinterpret_cast<const char*>(&data_size), 4);

                    // 组合WAV头部和音频数据
                    audio_data = wav_header.str() + decoded_data;
                }
            } else if (!config.audio.uri.empty()) {
                std::cerr << "Audio URI support not implemented yet" << std::endl;
                throw std::runtime_error("Audio URI support not implemented yet");
            } else {
                std::cerr << "No audio data provided" << std::endl;
                throw std::runtime_error("No audio data provided");
            }

            // 创建临时文件存储音频数据
            std::string temp_path = "/tmp/audio_" + generate_operation_id() + ".wav";
            std::cout << "Creating temporary file: " << temp_path << std::endl;
            
            std::ofstream temp_file(temp_path, std::ios::binary);
            if (!temp_file) {
                std::cerr << "Failed to create temporary file: " << temp_path << std::endl;
                throw std::runtime_error("Failed to create temporary file");
            }
            
            // 写入音频数据
            temp_file.write(audio_data.c_str(), audio_data.size());
            temp_file.close();
            std::cout << "Audio data written to temporary file" << std::endl;

            // 执行识别
            std::cout << "Starting recognition" << std::endl;
            auto result = recognizer_->recognize_sync(temp_path, config);
            std::cout << "Recognition completed" << std::endl;

            // 删除临时文件
            std::remove(temp_path.c_str());
            std::cout << "Temporary file removed" << std::endl;

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
            std::cout << "Response prepared" << std::endl;
            
            res.set_content(response.dump(), "application/json");
            std::cout << "Response sent" << std::endl;

        } catch (const json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "Error processing request: " << e.what() << std::endl;
            throw;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in handle_recognize: " << e.what() << std::endl;
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
        // 验证认证
        if (!verify_auth(req, res)) {
            return;
        }

        // 验证请求大小
        if (!verify_request_size(req, res)) {
            return;
        }

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