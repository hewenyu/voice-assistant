#include "pulse_audio_capture.h"
#include <audio/audio_format.h>
#include <common/model_config.h>
#include <recognizer/model_factory.h>
#include <iostream>

namespace linux_pulse {

PulseAudioCapture::PulseAudioCapture()
    : mainloop_(nullptr)
    , context_(nullptr)
    , stream_(nullptr)
    , is_recording_(false) {
    
    // 设置默认音频格式
    format_ = {16000, 1, 16};  // 16kHz, mono, 16-bit
    
    // 设置PulseAudio采样规格
    source_spec_.format = PA_SAMPLE_S16LE;
    source_spec_.rate = format_.sample_rate;
    source_spec_.channels = format_.channels;
    
    target_spec_ = source_spec_;
    // recognizer_ = std::make_unique<recognizer::Recognizer>(config);
}

PulseAudioCapture::~PulseAudioCapture() {
    cleanup();
}

void PulseAudioCapture::cleanup() {
    if (stream_) {
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
    }

    if (context_) {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        context_ = nullptr;
    }

    if (mainloop_) {
        pa_threaded_mainloop_stop(mainloop_);
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
    }
}

bool PulseAudioCapture::initialize() {
    mainloop_ = pa_threaded_mainloop_new();
    if (!mainloop_) {
        std::cerr << "Failed to create mainloop" << std::endl;
        return false;
    }

    context_ = pa_context_new(pa_threaded_mainloop_get_api(mainloop_),
                            "Voice Assistant Audio Capture");
    if (!context_) {
        std::cerr << "Failed to create context" << std::endl;
        cleanup();
        return false;
    }

    pa_context_set_state_callback(context_, context_state_cb, this);

    if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        std::cerr << "Failed to connect context" << std::endl;
        cleanup();
        return false;
    }

    pa_threaded_mainloop_start(mainloop_);

    // 初始化识别器
    // recognizer_->initialize();

    return true;
}

void PulseAudioCapture::context_state_cb(pa_context* c, void* userdata) {
    auto* capture = static_cast<PulseAudioCapture*>(userdata);
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(capture->mainloop_, 0);
            break;
        default:
            break;
    }
}

void PulseAudioCapture::stream_state_cb(pa_stream* s, void* userdata) {
    auto* capture = static_cast<PulseAudioCapture*>(userdata);
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(capture->mainloop_, 0);
            break;
        default:
            break;
    }
}

void PulseAudioCapture::stream_read_cb(pa_stream* s, size_t length, void* userdata) {
    auto* capture = static_cast<PulseAudioCapture*>(userdata);
    if (!capture->is_recording_) return;

    const void* data;
    if (pa_stream_peek(s, &data, &length) < 0) {
        std::cerr << "Failed to read from stream" << std::endl;
        return;
    }

    // if (data && length > 0 && capture->wav_writer_) {
    //     capture->wav_writer_->write(data, length);
    // }

    pa_stream_drop(s);
}

void PulseAudioCapture::sink_input_info_cb(pa_context* c, const pa_sink_input_info* i,
                                          int eol, void* userdata) {
    auto* capture = static_cast<PulseAudioCapture*>(userdata);
    if (eol > 0) return;
    if (!i) return;

    capture->available_applications_[i->index] = i->name ? i->name : "Unknown";
}

bool PulseAudioCapture::wait_for_operation(pa_operation* op) {
    if (!op) return false;

    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainloop_);
    }

    pa_operation_unref(op);
    return true;
}

void PulseAudioCapture::list_applications() {
    available_applications_.clear();

    pa_threaded_mainloop_lock(mainloop_);
    
    pa_operation* op = pa_context_get_sink_input_info_list(context_,
                                                          sink_input_info_cb,
                                                          this);
    wait_for_operation(op);

    pa_threaded_mainloop_unlock(mainloop_);

    std::cout << "Available applications:" << std::endl;
    for (const auto& app : available_applications_) {
        std::cout << "ID: " << app.first << ", Name: " << app.second << std::endl;
    }
}

bool PulseAudioCapture::start_recording_application(uint32_t app_id) {
    if (is_recording_) {
        std::cerr << "Already recording" << std::endl;
        return false;
    }

    auto it = available_applications_.find(app_id);
    if (it == available_applications_.end()) {
        std::cerr << "Application ID not found" << std::endl;
        return false;
    }

    app_name_ = it->second;

    pa_threaded_mainloop_lock(mainloop_);

    // 创建录制流
    stream_ = pa_stream_new(context_, "Record Stream", &source_spec_, nullptr);
    if (!stream_) {
        pa_threaded_mainloop_unlock(mainloop_);
        std::cerr << "Failed to create stream" << std::endl;
        return false;
    }

    pa_stream_set_state_callback(stream_, stream_state_cb, this);
    pa_stream_set_read_callback(stream_, stream_read_cb, this);

    // 连接到sink input
    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = (uint32_t)-1;
    buffer_attr.fragsize = (uint32_t)-1;

    if (pa_stream_connect_record(stream_, nullptr, &buffer_attr,
                               PA_STREAM_ADJUST_LATENCY) < 0) {
        pa_threaded_mainloop_unlock(mainloop_);
        std::cerr << "Failed to connect recording" << std::endl;
        return false;
    }

    is_recording_ = true;
    pa_threaded_mainloop_unlock(mainloop_);

    return true;
}

void PulseAudioCapture::stop_recording() {
    if (!is_recording_) return;

    is_recording_ = false;

    if (stream_) {
        pa_threaded_mainloop_lock(mainloop_);
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
        pa_threaded_mainloop_unlock(mainloop_);
    }
}

} // namespace voice 