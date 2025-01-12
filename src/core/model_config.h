#pragma once

#include <string>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <stdexcept>

namespace voice {

struct ModelConfig {
    // Basic configuration
    std::string provider = "cpu";
    int num_threads = 4;
    bool debug = false;

    // Model type
    std::string type = "sense_voice";  // "sense_voice" or "whisper"

    // Model configuration
    struct SenseVoiceConfig {
        std::string model_path;
        std::string tokens_path;
        std::string language = "auto";
        std::string decoding_method = "greedy_search";
        bool use_itn = true;
    } sense_voice;

    // Whisper model configuration
    struct WhisperConfig {
        std::string encoder_path;
        std::string decoder_path;
        std::string tokens_path;
        std::string language = "en";
        std::string task = "transcribe";  // "transcribe" or "translate"
        int tail_paddings = 0;
        std::string decoding_method = "greedy_search";
    } whisper;

    // VAD configuration
    struct VadConfig {
        std::string model_path;
        float threshold = 0.5f;
        float min_silence_duration = 0.5f;
        float min_speech_duration = 0.25f;
        float max_speech_duration = 5.0f;
        int window_size = 512;
        int sample_rate = 16000;
    } vad;

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
            } else {
                throw std::runtime_error("Unsupported model type: " + model_config.type);
            }

            // Load VAD configuration
            auto vad_config = config["vad"];
            model_config.vad.model_path = vad_config["model_path"].as<std::string>();
            model_config.vad.threshold = vad_config["threshold"].as<float>(0.5f);
            model_config.vad.min_silence_duration = vad_config["min_silence_duration"].as<float>(0.5f);
            model_config.vad.min_speech_duration = vad_config["min_speech_duration"].as<float>(0.25f);
            model_config.vad.max_speech_duration = vad_config["max_speech_duration"].as<float>(5.0f);
            model_config.vad.window_size = vad_config["window_size"].as<int>(512);
            model_config.vad.sample_rate = vad_config["sample_rate"].as<int>(16000);

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
    }
}; 

} // namespace voice 