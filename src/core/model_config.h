#pragma once

#include <string>

struct ModelConfig {
    // Basic model configuration
    std::string model_path;
    std::string tokens_path;
    std::string language = "auto";
    std::string provider = "cpu";
    std::string decoding_method = "greedy_search";
    int num_threads = 4;
    bool debug = false;
    bool use_itn = true;

    // VAD configuration
    std::string vad_model_path;
    float vad_threshold = 0.5f;
    float vad_min_silence_duration = 0.5f;
    float vad_min_speech_duration = 0.25f;
    float vad_max_speech_duration = 5.0f;
    int vad_window_size = 512;
    int sample_rate = 16000;

    // Error checking and validation
    bool validate() const {
        // Check required paths
        if (model_path.empty() || tokens_path.empty() || vad_model_path.empty()) {
            return false;
        }

        // Check VAD parameters
        if (vad_threshold < 0.0f || vad_threshold > 1.0f) {
            return false;
        }

        if (vad_min_silence_duration < 0.0f || vad_min_speech_duration < 0.0f || 
            vad_max_speech_duration < vad_min_speech_duration) {
            return false;
        }

        if (vad_window_size <= 0 || (vad_window_size & (vad_window_size - 1)) != 0) {
            // window_size should be positive and power of 2
            return false;
        }

        if (sample_rate <= 0) {
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
        if (model_path.empty()) {
            error += "Model path is empty\n";
        }
        if (tokens_path.empty()) {
            error += "Tokens path is empty\n";
        }
        if (vad_model_path.empty()) {
            error += "VAD model path is empty\n";
        }

        // Check VAD parameters
        if (vad_threshold < 0.0f || vad_threshold > 1.0f) {
            error += "VAD threshold should be between 0.0 and 1.0\n";
        }

        if (vad_min_silence_duration < 0.0f) {
            error += "Minimum silence duration should be positive\n";
        }
        if (vad_min_speech_duration < 0.0f) {
            error += "Minimum speech duration should be positive\n";
        }
        if (vad_max_speech_duration < vad_min_speech_duration) {
            error += "Maximum speech duration should be greater than minimum speech duration\n";
        }

        if (vad_window_size <= 0) {
            error += "Window size should be positive\n";
        } else if ((vad_window_size & (vad_window_size - 1)) != 0) {
            error += "Window size should be a power of 2\n";
        }

        if (sample_rate <= 0) {
            error += "Sample rate should be positive\n";
        }

        if (num_threads <= 0) {
            error += "Number of threads should be positive\n";
        }

        return error;
    }

    void set_defaults() {
        if (language.empty()) {
            language = "auto";
        }
        if (provider.empty()) {
            provider = "cpu";
        }
        if (decoding_method.empty()) {
            decoding_method = "greedy_search";
        }
        if (num_threads <= 0) {
            num_threads = 4;
        }
    }
}; 