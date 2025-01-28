#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <core/message_bus.h>
#include <core/message_types.h>
#include "audio_format.h"

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

    // Get current audio format
    virtual AudioFormat get_audio_format() const = 0;

    // Set message bus for audio data publishing
    virtual void set_message_bus(std::shared_ptr<core::MessageBus> message_bus) {
        message_bus_ = message_bus;
    }

    // Factory method to create audio capture instance
    static std::unique_ptr<IAudioCapture> CreateAudioCapture();

protected:
    // Helper method to publish audio data
    void publish_audio_data(const std::vector<float>& data, core::AudioMessage::Status status) {
        if (message_bus_) {
            auto msg = std::make_shared<core::AudioMessage>(
                data,
                get_audio_format().sample_rate,
                status
            );
            message_bus_->publish(msg);
        }
    }

    std::shared_ptr<core::MessageBus> message_bus_;
};

} // namespace audio 