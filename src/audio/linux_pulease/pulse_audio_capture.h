#pragma once

#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <map>
#include <memory>
#include <audio/audio_capture.h>
#include <audio/audio_format.h>
#include <common/model_config.h>
#include "sherpa-onnx/c-api/c-api.h"
#include "translator/translator.h"
namespace linux_pulse {

class PulseAudioCapture : public audio::IAudioCapture {

public:
    explicit PulseAudioCapture();
    ~PulseAudioCapture() override;

    // IAudioCapture interface implementation
    bool initialize() override;
    bool start_recording_application(uint32_t app_id) override;
    void stop_recording() override;
    void list_applications() override;
    void set_model_recognizer(const SherpaOnnxOfflineRecognizer* recognizer) override;
    void set_model_vad(SherpaOnnxVoiceActivityDetector* vad, const int window_size) override;
    void set_translate(const translator::ITranslator* translate) override;

private:
    // PulseAudio members
    pa_threaded_mainloop* mainloop_; // PulseAudio main loop
    pa_context* context_; // PulseAudio context
    pa_stream* stream_; // PulseAudio stream
    std::string app_name; // Name of the application to record
    bool is_recording; // Flag to indicate if recording is active
    std::vector<int16_t> audio_buffer;  // Buffer for audio data
    


    // Speech recognition members
    // const SherpaOnnxOfflineRecognizer* recognizer_;
    // const SherpaOnnxOfflineStream* recognition_stream_;
    // SherpaOnnxVoiceActivityDetector* vad_;
    // std::mutex recognition_mutex_;
    // bool recognition_enabled_;
    std::vector<float> remaining_samples_;  // Buffer for remaining samples between VAD windows
    
    std::map<std::string, std::string> available_sources;
    std::map<uint32_t, std::string> available_applications_;

    // Speech recognition members
    const SherpaOnnxOfflineRecognizer* recognizer_;
    const SherpaOnnxOfflineStream* recognition_stream_;
    SherpaOnnxVoiceActivityDetector* vad_;
    int window_size_;
    std::mutex recognition_mutex_;
    bool recognition_enabled_;


    // translate
    const translator::ITranslator* translate_;
    // Audio format settings
    audio::AudioFormat format_;
    pa_sample_spec source_spec_;
    pa_sample_spec target_spec_;

    static constexpr int SAMPLE_RATE = 16000;  // Required by VAD
    static constexpr int CHANNELS = 1;         // Mono for speech recognition
    static constexpr int BITS_PER_SAMPLE = 16; // S16LE format

    // Resampling state
    pa_sample_spec source_spec;
    pa_sample_spec target_spec;


    // Callback functions
    static void context_state_cb(pa_context* c, void* userdata);
    static void stream_state_cb(pa_stream* s, void* userdata);
    static void stream_read_cb(pa_stream* s, size_t length, void* userdata);
    static void sink_input_info_cb(pa_context* c, const pa_sink_input_info* i, int eol, void* userdata);

    // process_audio_for_recognition
    void process_audio_for_recognition(const std::vector<int16_t>& audio_data);

    // Helper functions
    void cleanup();
    bool wait_for_operation(pa_operation* op);
};

} // namespace voice 