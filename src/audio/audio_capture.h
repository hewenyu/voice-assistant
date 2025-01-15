#pragma once

#include <memory>
#include <functional>
#include "common/model_config.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "translator/translator.h"
namespace audio {

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    // Initialize audio capture
    virtual bool initialize() = 0;

    // Start recording from a specific application
    virtual bool start_recording_application(unsigned int pid) = 0;

    // Stop recording
    virtual void stop_recording() = 0;

    // List available applications
    virtual void list_applications() = 0;

    // Factory method to create audio capture instance
    static std::unique_ptr<IAudioCapture> CreateAudioCapture();

    // set model recognizerFactory method to create recognizer
    virtual void set_model_recognizer(const SherpaOnnxOfflineRecognizer* recognizer) = 0;

    // set model vad
    virtual void set_model_vad(const SherpaOnnxVoiceActivityDetector* vad, const int window_size) = 0;

    // set translate
    virtual void set_translate(const translator::ITranslator* translate) = 0;
};

} // namespace audio 