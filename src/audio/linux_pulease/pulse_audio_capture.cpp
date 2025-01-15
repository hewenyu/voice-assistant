#include "pulse_audio_capture.h"
#include <audio/audio_format.h>
#include <common/model_config.h>
#include <recognizer/model_factory.h>
#include <iostream>
#include "sherpa-onnx/c-api/c-api.h"
#include <mutex>
#include "translator/translator.h"
namespace linux_pulse {

PulseAudioCapture::PulseAudioCapture()
    : mainloop_(nullptr)
    , context_(nullptr)
    , stream_(nullptr)
    , is_recording(false) {
    
    // 设置默认音频格式
    format_ = {16000, 1, 16};  // 16kHz, mono, 16-bit
    
    // 设置PulseAudio采样规格
    source_spec_.format = PA_SAMPLE_S16LE;
    source_spec_.rate = format_.sample_rate;
    source_spec_.channels = format_.channels;
    
    target_spec_ = source_spec_;
}

PulseAudioCapture::~PulseAudioCapture() {
    cleanup();
}

void PulseAudioCapture::set_model_recognizer(const SherpaOnnxOfflineRecognizer* recognizer) {

    try {
        // check vad
        if (!vad_) {
            throw std::runtime_error("VAD is not initialized");
        }
        
        recognizer_ = recognizer;
        if (!recognizer_) {
            throw std::runtime_error("Recognizer is not initialized");
        }
        recognition_stream_ = SherpaOnnxCreateOfflineStream(recognizer_);
        if (!recognition_stream_) {
            throw std::runtime_error("Failed to create recognition stream");
        }
        recognition_enabled_ = true; // 证明这个模型可以识别

        
    } catch (const std::exception& e) {
        std::cerr << "Error setting model recognizer: " << e.what() << std::endl;
    }


}


// set model vad
void PulseAudioCapture::set_model_vad(const SherpaOnnxVoiceActivityDetector* vad, const int window_size) {
    vad_ = vad;
    window_size_ = window_size;
}

void PulseAudioCapture::set_translate(const translator::ITranslator* translate) {
    translate_ = translate;
}

// process_audio_for_recognition
void PulseAudioCapture::process_audio_for_recognition(const std::vector<int16_t>& audio_data) {
    if (!recognition_enabled_ || !vad_) {
        return;
    }
    std::lock_guard<std::mutex> lock(recognition_mutex_);
    // Convert to float samples
    std::vector<float> float_samples(audio_data.size());
    for (size_t i = 0; i < audio_data.size(); ++i) {
        float_samples[i] = audio_data[i] / 32768.0f;
    }

    // If we have remaining samples from last batch, prepend them
    if (!remaining_samples_.empty()) {
        float_samples.insert(float_samples.begin(),
                           remaining_samples_.begin(),
                           remaining_samples_.end());
        remaining_samples_.clear();
    }

    const int window_size = window_size_;
    size_t i = 0;
    while (i + window_size_ <= float_samples.size()) {
         // Feed window_size samples to VAD
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(
            vad_,
            float_samples.data() + i,
            window_size
        );
        // Process any complete speech segments
        while (!SherpaOnnxVoiceActivityDetectorEmpty(vad_)) {
            const SherpaOnnxSpeechSegment* segment = 
                    SherpaOnnxVoiceActivityDetectorFront(vad_);
            if (segment) {
                    // Create a new stream for this segment
                    const SherpaOnnxOfflineStream* stream = 
                        SherpaOnnxCreateOfflineStream(recognizer_);

                    if (stream) {
                        // Process the speech segment
                        SherpaOnnxAcceptWaveformOffline(
                            stream,
                            SAMPLE_RATE,
                            segment->samples,
                            segment->n
                        );

                        SherpaOnnxDecodeOfflineStream(recognizer_, stream);

                        const SherpaOnnxOfflineRecognizerResult* result = 
                            SherpaOnnxGetOfflineStreamResult(stream);

                        if (result && result->text) {
                            float start = segment->start / static_cast<float>(SAMPLE_RATE);
                            float duration = segment->n / static_cast<float>(SAMPLE_RATE);
                            float end = start + duration;

                            // 输出识别结果，包括时间戳和文本
                            // std::cout << "\n[Recognition Result]" << std::endl;
                            std::cout << "Time: " << std::fixed << std::setprecision(3)
                                      << start << "s -- " << end << "s" << std::endl;
                            std::cout << "Text: " << result->text << std::endl;
                            
                            // 如果有语言标识，也输出
                            if (result->lang) {
                                // std::cout << "Language: " << result->lang << std::endl;
                                // Language: <|zh|>
                                // 提取语言代码 例如 <|zh|> 提取 zh,并转换为大写
                                std::string language_code = std::string(result->lang).substr(2, 2);
                                std::transform(language_code.begin(), language_code.end(), language_code.begin(), ::toupper);
                                std::cout << "Language Code: " << language_code << std::endl;

                                // 输出 model_config_.deeplx.target_lang,也要大写
                                std::string target_lang = translate_->get_target_language();
                                std::transform(target_lang.begin(), target_lang.end(), target_lang.begin(), ::toupper);
                                std::cout << "Target Language: " << target_lang << std::endl;
                                // TODO: 翻译 target_lang 和 language_code 不一致时 ，都大写
                                if (target_lang != language_code && model_config_.deeplx.enabled) {
                                    std::string translated_text = translate_->translate(result->text, language_code);
                                    std::cout << "Translated Text: " << translated_text << std::endl;
                                }
                               
                            }
                            
                            // 如果有 tokens，也输出
                            // if (result->tokens) {
                            //     std::cout << "Tokens: " << result->tokens << std::endl;
                            // }
                            std::cout << std::string(50, '-') << std::endl;
                        }

                        // Clean up
                        SherpaOnnxDestroyOfflineRecognizerResult(result);
                        SherpaOnnxDestroyOfflineStream(stream);
                    }

                    SherpaOnnxDestroySpeechSegment(segment);
        }
    }

    // 将音频数据转换为浮点数
    // 将音频数据转换为浮点数
    std::vector<float> float_data(audio_data.begin(), audio_data.end());

    // 使用VAD进行处理
    vad_->AcceptWaveform(float_data.data(), float_data.size());
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
        throw std::runtime_error("Failed to create mainloop");
    }
            
    if (pa_threaded_mainloop_start(mainloop_) < 0) {
        pa_threaded_mainloop_free(mainloop_);
        throw std::runtime_error("Failed to start mainloop");
    }
            
    pa_threaded_mainloop_lock(mainloop_);
            
    context_ = pa_context_new(pa_threaded_mainloop_get_api(mainloop_), "AudioCapture");
    if (!context_) {
        pa_threaded_mainloop_unlock(mainloop_);
        pa_threaded_mainloop_stop(mainloop_);
        pa_threaded_mainloop_free(mainloop_);
        throw std::runtime_error("Failed to create context");
    }
            
    pa_context_set_state_callback(context_, context_state_cb, this);
            
    if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        pa_threaded_mainloop_unlock(mainloop_);
        pa_context_unref(context_);
        pa_threaded_mainloop_stop(mainloop_);
        pa_threaded_mainloop_free(mainloop_);
        throw std::runtime_error("Failed to connect context");
    }
            
    // Wait for context to be ready
    while (true) {
        pa_context_state_t state = pa_context_get_state(context_);
        if (state == PA_CONTEXT_READY) {
            break;
        }
        if (!PA_CONTEXT_IS_GOOD(state)) {
            pa_threaded_mainloop_unlock(mainloop_);
            pa_context_unref(context_);
            pa_threaded_mainloop_stop(mainloop_);
            pa_threaded_mainloop_free(mainloop_);
            throw std::runtime_error("Failed to connect to PulseAudio server");
        }
        pa_threaded_mainloop_wait(mainloop_);
    }
            
    pa_threaded_mainloop_unlock(mainloop_);

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
    if (!context_ || pa_context_get_state(context_) != PA_CONTEXT_READY) {
        throw std::runtime_error("PulseAudio context not ready");
    }

    available_applications_.clear();
            
    pa_threaded_mainloop_lock(mainloop_);
            
    pa_operation *op = pa_context_get_sink_input_info_list(context_, sink_input_info_cb, this);
    if (!op) {
        pa_threaded_mainloop_unlock(mainloop_);
        throw std::runtime_error("Failed to get sink input info list");
    }
            
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainloop_);
    }
            
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(mainloop_);
            
    if (available_applications_.empty()) {
        std::cout << "No applications are currently playing audio." << std::endl;
    } else {
        std::cout << "Applications currently playing audio:" << std::endl;
        for (const auto& app : available_applications_) {
            std::cout << "  " << app.first << ": " << app.second << std::endl;
        }
    }
}

bool PulseAudioCapture::start_recording_application(uint32_t sink_input_index) {
     std::cout << "Starting recording for sink input " << sink_input_index << std::endl;
            
    if (stream_) {
        throw std::runtime_error("Already recording");
    }

    pa_threaded_mainloop_lock(mainloop_);
    std::cout << "Mainloop locked" << std::endl;

    // Set up source format
    source_spec.format = PA_SAMPLE_S16LE;
    source_spec.channels = 2;  // Default to stereo
    source_spec.rate = 16000;  // Changed to 16kHz
            
    std::cout << "Source format: " << source_spec.rate << "Hz, " 
              << source_spec.channels << " channels" << std::endl;
            
    // Create stream
    stream_ = pa_stream_new(context_, "RecordStream", &source_spec, nullptr);
    if (!stream_) {
        pa_threaded_mainloop_unlock(mainloop_);
        throw std::runtime_error("Failed to create stream");
    }
    std::cout << "Stream created" << std::endl;
            
    pa_stream_set_state_callback(stream_, stream_state_cb, mainloop_);
    pa_stream_set_read_callback(stream_, stream_read_cb, this);
            
    // Set up buffer attributes (following OBS's approach)
    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = (uint32_t)-1;
    buffer_attr.fragsize = pa_usec_to_bytes(25000, &source_spec);  // 25ms chunks
    buffer_attr.minreq = (uint32_t)-1;
    buffer_attr.prebuf = (uint32_t)-1;
    buffer_attr.tlength = (uint32_t)-1;
            
    std::cout << "Buffer attributes set up with fragsize: " << buffer_attr.fragsize << std::endl;
            
    // Get the sink name for this application
    bool found = false;
    std::string sink_name;
            
    std::cout << "Getting sink info for input " << sink_input_index << std::endl;

    struct CallbackData {
        PulseAudioCapture* ac;
        bool* found;
        std::string* sink_name;
        pa_stream* stream;
    };
            
    auto get_sink_cb = [](pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
        std::cout << "Sink info callback called with eol: " << eol << std::endl;
        auto* data = static_cast<CallbackData*>(userdata);
        
        if (!eol && i) {
            *data->found = true;
            *data->sink_name = i->sink;
            std::cout << "Found sink: " << i->sink << std::endl;
        } else if (eol < 0) {
            // Error occurred
            std::cerr << "Error getting sink info" << std::endl;
        }
        
        pa_threaded_mainloop_signal(data->ac->mainloop_, 0);
    };
            
    CallbackData cb_data{this, &found, &sink_name, stream_};
            
    pa_operation *op = pa_context_get_sink_input_info(context_, sink_input_index, get_sink_cb, &cb_data);
    if (!op) {
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
        pa_threaded_mainloop_unlock(mainloop_);
        throw std::runtime_error("Failed to get sink input info");
    }
            
    std::cout << "Waiting for sink info operation to complete" << std::endl;
            
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainloop_);
    }
    pa_operation_unref(op);
            
    if (!found) {
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
        pa_threaded_mainloop_unlock(mainloop_);
        throw std::runtime_error("Failed to find sink for application");
    }
            
    std::cout << "Found sink: " << sink_name << std::endl;
            
    // Connect to the monitor source of the sink
    std::string monitor_source = sink_name + ".monitor";
    std::cout << "Connecting to monitor source: " << monitor_source << std::endl;
            
    if (pa_stream_connect_record(stream_, monitor_source.c_str(), &buffer_attr,
        static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE)) < 0) {
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
        pa_threaded_mainloop_unlock(mainloop_);
        throw std::runtime_error("Failed to connect stream");
    }
            
    std::cout << "Stream connected successfully" << std::endl;
            
    pa_threaded_mainloop_unlock(mainloop_);
    is_recording = true;
            
    // Clear any existing audio data
    audio_buffer.clear();
    std::cout << "Recording started" << std::endl;
}

void PulseAudioCapture::stop_recording() {
    is_recording = false;
    if (stream_) {
        pa_threaded_mainloop_lock(mainloop_);
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
        pa_threaded_mainloop_unlock(mainloop_);
    }
     
}

} // namespace linux_pulse 