#pragma once

#include <memory>
#include <string>
#include "model_config.h"
#include "sherpa-onnx/c-api/c-api.h"

namespace voice {

class ModelFactory {
public:
    static const SherpaOnnxOfflineRecognizer* CreateModel(const ModelConfig& config) {
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
            whisper_config.language = config.whisper.language.c_str();
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

} // namespace voice 