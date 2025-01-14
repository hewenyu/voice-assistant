#pragma once

#include <memory>
#include <string>
#include <iostream>
#include "model_config.h"
#include "sherpa-onnx/c-api/c-api.h"

namespace recognizer {

class ModelFactory {
public:
    static std::string DetectLanguage(const common::ModelConfig& config, const float* samples, int32_t n) {
        // Create language identification config using whisper configuration
        SherpaOnnxSpokenLanguageIdentificationConfig slid_config = {};
        
        // Use whisper configuration for language detection
        slid_config.whisper.encoder = config.whisper.encoder_path.c_str();
        slid_config.whisper.decoder = config.whisper.decoder_path.c_str();
        slid_config.num_threads = config.whisper.language_detection_num_threads;
        slid_config.provider = config.whisper.language_detection_provider.c_str();
        slid_config.debug = config.whisper.language_detection_debug ? 1 : 0;

        // Create language identification instance
        const SherpaOnnxSpokenLanguageIdentification* slid = 
            SherpaOnnxCreateSpokenLanguageIdentification(&slid_config);
        if (!slid) {
            throw std::runtime_error("Failed to create language identification");
        }

        // Create stream for language identification
        SherpaOnnxOfflineStream* stream = 
            SherpaOnnxSpokenLanguageIdentificationCreateOfflineStream(slid);
        if (!stream) {
            SherpaOnnxDestroySpokenLanguageIdentification(slid);
            throw std::runtime_error("Failed to create stream for language identification");
        }

        // Process audio samples
        SherpaOnnxAcceptWaveformOffline(stream, 16000, samples, n);

        // Get detected language
        const SherpaOnnxSpokenLanguageIdentificationResult* result = 
            SherpaOnnxSpokenLanguageIdentificationCompute(slid, stream);
        if (!result) {
            SherpaOnnxDestroyOfflineStream(stream);
            SherpaOnnxDestroySpokenLanguageIdentification(slid);
            throw std::runtime_error("Failed to detect language");
        }

        std::string detected_language = result->lang;

        // Cleanup
        SherpaOnnxDestroySpokenLanguageIdentificationResult(result);
        SherpaOnnxDestroyOfflineStream(stream);
        SherpaOnnxDestroySpokenLanguageIdentification(slid);

        return detected_language;
    }

    static const SherpaOnnxOfflineRecognizer* CreateModel(
        const common::ModelConfig& config,
        const float* samples = nullptr,
        int32_t n = 0) {
        // Zero initialization
        SherpaOnnxOfflineRecognizerConfig recognizer_config = {};
        SherpaOnnxOfflineModelConfig model_config = {};

        // Set common configurations
        model_config.debug = config.debug ? 1 : 0;
        model_config.num_threads = config.num_threads;
        model_config.provider = config.provider.c_str();

        if (config.type == "sense_voice") {
            // Configure for sense_voice model
            model_config.tokens = config.sense_voice.tokens_path.c_str();
            
            SherpaOnnxOfflineSenseVoiceModelConfig sense_voice_config = {};
            sense_voice_config.model = config.sense_voice.model_path.c_str();
            sense_voice_config.language = config.sense_voice.language.c_str();
            sense_voice_config.use_itn = config.sense_voice.use_itn ? 1 : 0;
            
            model_config.sense_voice = sense_voice_config;
            recognizer_config.decoding_method = config.sense_voice.decoding_method.c_str();
        } else if (config.type == "whisper") {
            // Configure for whisper model
            model_config.tokens = config.whisper.tokens_path.c_str();
            
            SherpaOnnxOfflineWhisperModelConfig whisper_config = {};
            whisper_config.encoder = config.whisper.encoder_path.c_str();
            whisper_config.decoder = config.whisper.decoder_path.c_str();
            
            // Handle auto language detection only for whisper model
            std::string language = config.whisper.language;
            if (language == "auto" && config.whisper.enable_language_detection && samples != nullptr && n > 0) {
                try {
                    language = DetectLanguage(config, samples, n);
                    std::cout << "Detected language: " << language << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Language detection failed: " << e.what() << std::endl;
                    language = "en";  // Default to English on failure
                }
            } else if (language == "auto") {
                language = "en";  // Default to English if no samples provided or language detection disabled
            }
            whisper_config.language = language.c_str();
            whisper_config.task = config.whisper.task.c_str();
            whisper_config.tail_paddings = config.whisper.tail_paddings;
            
            model_config.whisper = whisper_config;
            recognizer_config.decoding_method = config.whisper.decoding_method.c_str();
        } else {
            throw std::runtime_error("Unsupported model type: " + config.type);
        }

        recognizer_config.model_config = model_config;
        return SherpaOnnxCreateOfflineRecognizer(&recognizer_config);
    }
};

} // namespace recognizer 