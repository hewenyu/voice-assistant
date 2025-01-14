#pragma once

#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <map>
#include <memory>
#include <audio/audio_capture.h>
#include <audio/audio_format.h>
#include <common/model_config.h>
#include <recognizer/recognizer.h>

namespace linux_pulse {

class PulseAudioCapture : public audio::IAudioCapture {

public:
    explicit PulseAudioCapture(const common::ModelConfig& config);
    ~PulseAudioCapture() override;

    // IAudioCapture interface implementation
    bool initialize() override;
    bool start_recording_application(uint32_t app_id) override;
    void stop_recording() override;
    void list_applications() override;

private:
    pa_threaded_mainloop* mainloop_;
    pa_context* context_;
    pa_stream* stream_;
    std::string app_name_;
    bool is_recording_;
    

    std::map<std::string, std::string> available_sources_;
    std::map<uint32_t, std::string> available_applications_;
    // Audio format settings
    audio::AudioFormat format_;
    pa_sample_spec source_spec_;
    pa_sample_spec target_spec_;



    static constexpr int SAMPLE_RATE = 16000;  // Required by VAD
    static constexpr int CHANNELS = 1;         // Mono for speech recognition
    static constexpr int BITS_PER_SAMPLE = 16; // S16LE format

    // recognizer_
    std::unique_ptr<recognizer::Recognizer> recognizer_;

    // Callback functions
    static void context_state_cb(pa_context* c, void* userdata);
    static void stream_state_cb(pa_stream* s, void* userdata);
    static void stream_read_cb(pa_stream* s, size_t length, void* userdata);
    static void sink_input_info_cb(pa_context* c, const pa_sink_input_info* i, int eol, void* userdata);

    // Helper functions
    void cleanup();
    bool wait_for_operation(pa_operation* op);
};

} // namespace voice 