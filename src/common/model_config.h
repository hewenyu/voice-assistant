#pragma once

#include <string>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <stdexcept>

namespace common {

struct WhisperConfig {
    std::string encoder_path;
    std::string decoder_path;
    std::string tokens_path;
    std::string language = "en";  // "en", "zh", "ja", "ko", etc.
    std::string task = "transcribe";  // "transcribe" or "translate"
    int tail_paddings = 0;
    std::string decoding_method = "greedy_search";
    
    // Language detection settings
    bool enable_language_detection = false;  // Only used when language is "auto"
    int language_detection_num_threads = 1;
    std::string language_detection_provider = "cpu";
    bool language_detection_debug = false;
};

struct SenseVoiceConfig {
    std::string model_path;
    std::string tokens_path;
    std::string language = "auto";  // "auto", "zh", "en", "ja", "ko", "yue"
    std::string decoding_method = "greedy_search";
    bool use_itn = true;
};

struct VadConfig {
    std::string model_path;
    float threshold = 0.3;
    float min_silence_duration = 0.25;
    float min_speech_duration = 0.1;
    float max_speech_duration = 15.0;
    int window_size = 256;
    int sample_rate = 16000;
    int num_threads = 1;
    bool debug = false;
};

struct DeepLXConfig {
    std::string url;
    std::string token;
    std::string target_lang = "ZH";  // Default target language is Chinese
    bool enabled = false;  // Whether translation is enabled
};

struct ModelConfig {
    std::string type;  // "sense_voice" or "whisper"
    std::string provider = "cpu";
    int num_threads = 4;
    bool debug = false;

    // Model specific configurations
    WhisperConfig whisper;
    SenseVoiceConfig sense_voice;
    VadConfig vad;
    DeepLXConfig deeplx;  // Add DeepLX configuration

    // Load configuration from YAML file
    static ModelConfig LoadFromFile(const std::string& config_path) {
        try {
            YAML::Node config = YAML::LoadFile(config_path);
            ModelConfig model_config;

            // Load basic configuration
            model_config.provider = config["provider"].as<std::string>("cpu");
            model_config.num_threads = config["num_threads"].as<int>(4);
            model_config.debug = config["debug"].as<bool>(false);

            // Load model type
            if (!config["model"] || !config["model"]["type"]) {
                throw std::runtime_error("Model type must be specified");
            }
            model_config.type = config["model"]["type"].as<std::string>();

            // Load model-specific configuration
            if (model_config.type == "sense_voice") {
                auto sense_voice_config = config["model"]["sense_voice"];
                model_config.sense_voice.model_path = sense_voice_config["model_path"].as<std::string>();
                model_config.sense_voice.tokens_path = sense_voice_config["tokens_path"].as<std::string>();
                model_config.sense_voice.language = sense_voice_config["language"].as<std::string>("auto");
                model_config.sense_voice.decoding_method = sense_voice_config["decoding_method"].as<std::string>("greedy_search");
                model_config.sense_voice.use_itn = sense_voice_config["use_itn"].as<bool>(true);
            } else if (model_config.type == "whisper") {
                auto whisper_config = config["model"]["whisper"];
                model_config.whisper.encoder_path = whisper_config["encoder_path"].as<std::string>();
                model_config.whisper.decoder_path = whisper_config["decoder_path"].as<std::string>();
                model_config.whisper.tokens_path = whisper_config["tokens_path"].as<std::string>();
                model_config.whisper.language = whisper_config["language"].as<std::string>("en");
                model_config.whisper.task = whisper_config["task"].as<std::string>("transcribe");
                model_config.whisper.tail_paddings = whisper_config["tail_paddings"].as<int>(0);
                model_config.whisper.decoding_method = whisper_config["decoding_method"].as<std::string>("greedy_search");
                
                // Load language detection settings if language is "auto"
                if (model_config.whisper.language == "auto") {
                    model_config.whisper.enable_language_detection = true;
                    model_config.whisper.language_detection_num_threads = 
                        whisper_config["language_detection_num_threads"].as<int>(1);
                    model_config.whisper.language_detection_provider = 
                        whisper_config["language_detection_provider"].as<std::string>("cpu");
                    model_config.whisper.language_detection_debug = 
                        whisper_config["language_detection_debug"].as<bool>(false);
                }
            } else {
                throw std::runtime_error("Unsupported model type: " + model_config.type);
            }

            // Load VAD configuration
            auto vad_config = config["vad"];
            model_config.vad.model_path = vad_config["model_path"].as<std::string>();
            model_config.vad.threshold = vad_config["threshold"].as<float>(0.3f);
            model_config.vad.min_silence_duration = vad_config["min_silence_duration"].as<float>(0.25f);
            model_config.vad.min_speech_duration = vad_config["min_speech_duration"].as<float>(0.1f);
            model_config.vad.max_speech_duration = vad_config["max_speech_duration"].as<float>(15.0f);
            model_config.vad.window_size = vad_config["window_size"].as<int>(256);
            model_config.vad.sample_rate = vad_config["sample_rate"].as<int>(16000);
            model_config.vad.num_threads = vad_config["num_threads"].as<int>(1);
            model_config.vad.debug = vad_config["debug"].as<bool>(false);

            // Load DeepLX configuration if present
            if (config["deeplx"]) {
                auto deeplx_config = config["deeplx"];
                model_config.deeplx.enabled = deeplx_config["enabled"].as<bool>(false);
                if (model_config.deeplx.enabled) {
                    model_config.deeplx.url = deeplx_config["url"].as<std::string>();
                    model_config.deeplx.token = deeplx_config["token"].as<std::string>();
                    model_config.deeplx.target_lang = deeplx_config["target_lang"].as<std::string>("ZH");
                }
            }

            return model_config;
        } catch (const YAML::Exception& e) {
            throw std::runtime_error("Failed to parse config file: " + std::string(e.what()));
        }
    }

    std::string Validate() const {
        std::string error;

        // Validate model type
        if (type != "sense_voice" && type != "whisper") {
            error += "Model type must be either 'sense_voice' or 'whisper'\n";
        }

        // Validate model-specific configuration
        if (type == "sense_voice") {
            if (sense_voice.model_path.empty()) {
                error += "SenseVoice model path is empty\n";
            }
            if (sense_voice.tokens_path.empty()) {
                error += "SenseVoice tokens path is empty\n";
            }
        } else if (type == "whisper") {
            if (whisper.encoder_path.empty()) {
                error += "Whisper encoder path is empty\n";
            }
            if (whisper.decoder_path.empty()) {
                error += "Whisper decoder path is empty\n";
            }
            if (whisper.tokens_path.empty()) {
                error += "Whisper tokens path is empty\n";
            }
            if (whisper.task != "transcribe" && whisper.task != "translate") {
                error += "Whisper task must be either 'transcribe' or 'translate'\n";
            }
        }

        // Validate VAD configuration
        if (vad.model_path.empty()) {
            error += "VAD model path is empty\n";
        }
        if (vad.threshold < 0.0f || vad.threshold > 1.0f) {
            error += "VAD threshold should be between 0.0 and 1.0\n";
        }
        if (vad.min_silence_duration < 0.0f) {
            error += "Minimum silence duration should be positive\n";
        }
        if (vad.min_speech_duration < 0.0f) {
            error += "Minimum speech duration should be positive\n";
        }
        if (vad.max_speech_duration < vad.min_speech_duration) {
            error += "Maximum speech duration should be greater than minimum speech duration\n";
        }
        if (vad.window_size <= 0) {
            error += "Window size should be positive\n";
        } else if ((vad.window_size & (vad.window_size - 1)) != 0) {
            error += "Window size should be a power of 2\n";
        }
        if (vad.sample_rate <= 0) {
            error += "Sample rate should be positive\n";
        }
        if (num_threads <= 0) {
            error += "Number of threads should be positive\n";
        }

        // Validate DeepLX configuration if enabled
        if (deeplx.enabled) {
            if (deeplx.url.empty()) {
                error += "DeepLX URL is empty\n";
            }
            if (deeplx.token.empty()) {
                error += "DeepLX token is empty\n";
            }
            if (deeplx.target_lang.empty()) {
                error += "DeepLX target language is empty\n";
            }
        }

        return error;
    }

    void set_defaults() {
        if (type.empty()) {
            type = "sense_voice";
        }
        if (provider.empty()) {
            provider = "cpu";
        }
        if (num_threads <= 0) {
            num_threads = 4;
        }

        // Set model-specific defaults
        if (type == "sense_voice") {
            if (sense_voice.language.empty()) {
                sense_voice.language = "auto";
            }
            if (sense_voice.decoding_method.empty()) {
                sense_voice.decoding_method = "greedy_search";
            }
        } else if (type == "whisper") {
            if (whisper.language.empty()) {
                whisper.language = "en";
            }
            if (whisper.task.empty()) {
                whisper.task = "transcribe";
            }
            if (whisper.decoding_method.empty()) {
                whisper.decoding_method = "greedy_search";
            }
        }

        // Set DeepLX defaults
        if (deeplx.target_lang.empty()) {
            deeplx.target_lang = "ZH";
        }
    }
}; 

} // namespace voice 