#pragma once

#include <string>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <stdexcept>

struct ModelConfig {
    // Basic configuration
    std::string provider = "cpu";
    int num_threads = 4;
    bool debug = false;

    // Model configuration
    struct SenseVoiceConfig {
        std::string model_path;
        std::string tokens_path;
        std::string language = "auto";
        std::string decoding_method = "greedy_search";
        bool use_itn = true;
    } sense_voice;

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

            // Load model configuration
            if (!config["model"] || config["model"]["type"].as<std::string>() != "sense_voice") {
                throw std::runtime_error("Model type must be 'sense_voice'");
            }

            auto sense_voice_config = config["model"]["sense_voice"];
            model_config.sense_voice.model_path = sense_voice_config["model_path"].as<std::string>();
            model_config.sense_voice.tokens_path = sense_voice_config["tokens_path"].as<std::string>();
            model_config.sense_voice.language = sense_voice_config["language"].as<std::string>("auto");
            model_config.sense_voice.decoding_method = sense_voice_config["decoding_method"].as<std::string>("greedy_search");
            model_config.sense_voice.use_itn = sense_voice_config["use_itn"].as<bool>(true);

            // Load VAD configuration
            auto vad_config = config["vad"];
            model_config.vad.model_path = vad_config["model_path"].as<std::string>();
            model_config.vad.threshold = vad_config["threshold"].as<float>(0.5f);
            model_config.vad.min_silence_duration = vad_config["min_silence_duration"].as<float>(0.5f);
            model_config.vad.min_speech_duration = vad_config["min_speech_duration"].as<float>(0.25f);
            model_config.vad.max_speech_duration = vad_config["max_speech_duration"].as<float>(5.0f);
            model_config.vad.window_size = vad_config["window_size"].as<int>(512);
            model_config.vad.sample_rate = vad_config["sample_rate"].as<int>(16000);

            // Validate configuration
            if (!model_config.validate()) {
                throw std::runtime_error("Invalid configuration: " + model_config.get_error_message());
            }

            return model_config;
        } catch (const YAML::Exception& e) {
            throw std::runtime_error("Failed to load config file: " + std::string(e.what()));
        }
    }

    // Error checking and validation
    bool validate() const {
        // Check required paths
        if (sense_voice.model_path.empty() || sense_voice.tokens_path.empty() || vad.model_path.empty()) {
            return false;
        }

        // Check VAD parameters
        if (vad.threshold < 0.0f || vad.threshold > 1.0f) {
            return false;
        }

        if (vad.min_silence_duration < 0.0f || vad.min_speech_duration < 0.0f || 
            vad.max_speech_duration < vad.min_speech_duration) {
            return false;
        }

        if (vad.window_size <= 0 || (vad.window_size & (vad.window_size - 1)) != 0) {
            // window_size should be positive and power of 2
            return false;
        }

        if (vad.sample_rate <= 0) {
            return false;
        }

        if (num_threads <= 0) {
            return false;
        }

        return true;
    }

    std::string get_error_message() const {
        std::string error;

        // Check required paths
        if (sense_voice.model_path.empty()) {
            error += "Model path is empty\n";
        }
        if (sense_voice.tokens_path.empty()) {
            error += "Tokens path is empty\n";
        }
        if (vad.model_path.empty()) {
            error += "VAD model path is empty\n";
        }

        // Check VAD parameters
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
        if (sense_voice.language.empty()) {
            sense_voice.language = "auto";
        }
        if (provider.empty()) {
            provider = "cpu";
        }
        if (sense_voice.decoding_method.empty()) {
            sense_voice.decoding_method = "greedy_search";
        }
        if (num_threads <= 0) {
            num_threads = 4;
        }
    }
}; 