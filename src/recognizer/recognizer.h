#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include "sherpa-onnx/c-api/c-api.h"
#include <common/model_config.h>
#include <translator/translator.h>

namespace recognizer {

class Recognizer {
public:
    Recognizer(const common::ModelConfig& config);
    ~Recognizer();

    // Initialize the recognizer with model configuration
    bool initialize();

    // Process audio data for recognition
    void process_audio(const std::vector<float>& audio_data);

    // Get the current recognition state
    bool is_enabled() const { return recognition_enabled_; }

private:
    // Speech recognition members
    const SherpaOnnxOfflineRecognizer* recognizer_;
    const SherpaOnnxOfflineStream* recognition_stream_;
    SherpaOnnxVoiceActivityDetector* vad_;
    std::mutex recognition_mutex_;
    bool recognition_enabled_;
    std::vector<float> remaining_samples_;  // Buffer for remaining samples between VAD windows

    // Configuration
    common::ModelConfig model_config_;

    // Helper functions
    void process_speech_segment(const SherpaOnnxSpeechSegment* segment);
    std::string translate(const std::string& text, const std::string& source_lang);

    // Translation support
    std::unique_ptr<translator::ITranslator> translator_;
};

} // namespace recognizer
