#pragma once

#include <memory>
#include <functional>
#include "common/model_config.h"

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
    static std::unique_ptr<IAudioCapture> CreateAudioCapture(common::ModelConfig& config);
};

} // namespace audio 