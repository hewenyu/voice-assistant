#pragma once

#include "audio_capture.h"
#include "wav_writer.h"
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <map>
#include <memory>

namespace voice {

class PulseAudioCapture : public IAudioCapture {
public:
    explicit PulseAudioCapture(const std::string& config_path = "");
    ~PulseAudioCapture() override;

    // IAudioCapture interface implementation
    bool initialize() override;
    bool start_recording_application(uint32_t app_id, const std::string& output_path = "") override;
    void stop_recording() override;
    void list_applications() override;

private:
    pa_threaded_mainloop* mainloop_;
    pa_context* context_;
    pa_stream* stream_;
    std::string app_name_;
    bool is_recording_;
    
    std::unique_ptr<WavWriter> wav_writer_;
    std::map<uint32_t, std::string> available_applications_;

    // Audio format settings
    AudioFormat format_;
    pa_sample_spec source_spec_;
    pa_sample_spec target_spec_;

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