#include "pulse_audio_capture.h"
#include <audio/audio_format.h>
#include <common/model_config.h>
#include <recognizer/model_factory.h>
#include <iostream>
#include <iomanip>
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
            std::cerr << "[ERROR] VAD is not initialized" << std::endl;
            throw std::runtime_error("VAD is not initialized");
        }
        
        recognizer_ = recognizer;
        if (!recognizer_) {
            std::cerr << "[ERROR] Recognizer is not initialized" << std::endl;
            throw std::runtime_error("Recognizer is not initialized");
        }
        
        recognition_stream_ = SherpaOnnxCreateOfflineStream(recognizer_);
        if (!recognition_stream_) {
            std::cerr << "[ERROR] Failed to create recognition stream" << std::endl;
            throw std::runtime_error("Failed to create recognition stream");
        }
        
        recognition_enabled_ = true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error setting model recognizer: " << e.what() << std::endl;
        throw;
    }
}


// set model vad
void PulseAudioCapture::set_model_vad(SherpaOnnxVoiceActivityDetector* vad, const int window_size) {
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
    while (i + window_size <= float_samples.size()) {
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

                        std::cout << "\n[Recognition Result]" << std::endl;
                        std::cout << "Time: " << std::fixed << std::setprecision(3)
                                  << start << "s -- " << end << "s" << std::endl;
                        std::cout << "Text: " << result->text << std::endl;

                        if (result->lang) {
                            std::string language_code = std::string(result->lang).substr(2, 2);
                            std::transform(language_code.begin(), language_code.end(), language_code.begin(), ::toupper);
                            std::cout << "Language Code: " << language_code << std::endl;

                            std::string target_lang = translate_->get_target_language();
                            std::transform(target_lang.begin(), target_lang.end(), target_lang.begin(), ::toupper);
                            std::cout << "Target Language: " << target_lang << std::endl;
                            
                            if (target_lang != language_code) {
                                try {
                                    std::string translated_text = translate_->translate(result->text, language_code);
                                    std::cout << "Translated Text: " << translated_text << std::endl;
                                } catch (const std::exception& e) {
                                    std::cerr << "Error translating text: " << e.what() << std::endl;
                                }
                            }
                        }
                        std::cout << std::string(50, '-') << std::endl;
                    } else {
                        std::cout << "No recognition result or empty text" << std::endl;
                    }

                    // Clean up
                    SherpaOnnxDestroyOfflineRecognizerResult(result);
                    SherpaOnnxDestroyOfflineStream(stream);
                } else {
                    std::cerr << "[ERROR] Failed to create stream for speech segment" << std::endl;
                }

                SherpaOnnxDestroySpeechSegment(segment);
            }
            SherpaOnnxVoiceActivityDetectorPop(vad_);
        }
        i += window_size;
    }

    // Store remaining samples for next batch
    if (i < float_samples.size()) {
        remaining_samples_.assign(
            float_samples.begin() + i,
            float_samples.end()
        );
    }
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

void PulseAudioCapture::stream_state_cb(pa_stream* /*s*/, void* userdata) {
    auto *mainloop = static_cast<pa_threaded_mainloop*>(userdata);
    pa_threaded_mainloop_signal(mainloop, 0);
}

void PulseAudioCapture::stream_read_cb(pa_stream* s, size_t /*length*/, void* userdata) {
    auto *ac = static_cast<PulseAudioCapture*>(userdata);
    const void *data;
    size_t bytes;
    
    if (pa_stream_peek(s, &data, &bytes) < 0) {
        std::cerr << "Failed to read from stream" << std::endl;
        return;
    }
    
    if (!data) {
        if (bytes > 0) {
            std::cerr << "Got audio hole of " << bytes << " bytes" << std::endl;
        }
        pa_stream_drop(s);
        return;
    }
    
    if (bytes > 0 && ac->is_recording) {
        // std::cout << "Processing " << bytes << " bytes of audio data" << std::endl;
        // Convert audio data to the required format (16kHz, mono, S16LE)
        const int16_t *samples = static_cast<const int16_t*>(data);
        size_t num_samples = bytes / sizeof(int16_t);
        
        // std::cout << "Number of samples: " << num_samples << std::endl;
        
        // If stereo, convert to mono by averaging channels
        if (ac->source_spec.channels == 2) {
            // std::cout << "Converting stereo to mono" << std::endl;
            for (size_t i = 0; i < num_samples; i += 2) {
                int32_t mono_sample = (static_cast<int32_t>(samples[i]) + 
                                     static_cast<int32_t>(samples[i + 1])) / 2;
                ac->audio_buffer.push_back(static_cast<int16_t>(mono_sample));
            }
        } else {
            std::cout << "Copying mono samples directly" << std::endl;
            ac->audio_buffer.insert(ac->audio_buffer.end(), samples, samples + num_samples);
        }
        
        // Resample if needed (simple linear resampling)
        if (ac->source_spec.rate != SAMPLE_RATE) {
            std::cout << "Resampling from " << ac->source_spec.rate << " to " << SAMPLE_RATE << std::endl;
            std::vector<int16_t> resampled;
            float ratio = static_cast<float>(SAMPLE_RATE) / ac->source_spec.rate;
            size_t new_size = static_cast<size_t>(ac->audio_buffer.size() * ratio);
            resampled.reserve(new_size);
            
            for (size_t i = 0; i < new_size; ++i) {
                float src_idx = i / ratio;
                size_t idx1 = static_cast<size_t>(src_idx);
                size_t idx2 = idx1 + 1;
                if (idx2 >= ac->audio_buffer.size()) idx2 = idx1;
                
                float frac = src_idx - idx1;
                int16_t sample = static_cast<int16_t>(
                    ac->audio_buffer[idx1] * (1.0f - frac) + 
                    ac->audio_buffer[idx2] * frac
                );
                resampled.push_back(sample);
            }
            
            ac->audio_buffer = std::move(resampled);
        }
        
        // std::cout << "Final buffer size: " << ac->audio_buffer.size() << " samples" << std::endl;
    
        
        if (ac->recognition_enabled_) {
                // std::cout << "Processing audio for recognition" << std::endl;
                ac->process_audio_for_recognition(ac->audio_buffer);
            }
        
        ac->audio_buffer.clear();  // Clear buffer after processing
    }
    
    pa_stream_drop(s);
}

void PulseAudioCapture::sink_input_info_cb(pa_context* /*c*/, const pa_sink_input_info* i,
                                        int eol, void* userdata) {
    if (!eol && i) {
        auto *ac = static_cast<PulseAudioCapture*>(userdata);
        
        // Get detailed information from property list
        std::string app_name = "Unknown";
        
        // Try different properties to get the most descriptive name
        const char* media_name = pa_proplist_gets(i->proplist, "media.name");
        const char* application_name = pa_proplist_gets(i->proplist, "application.name");
        const char* application_process_name = pa_proplist_gets(i->proplist, "application.process.name");
        const char* window_title = pa_proplist_gets(i->proplist, "window.title");
        const char* media_title = pa_proplist_gets(i->proplist, "media.title");
        const char* media_role = pa_proplist_gets(i->proplist, "media.role");
        const char* stream_name = i->name;

                    // Debug output
        std::cout << "Properties for sink input " << i->index << ":\n";
        std::cout << "  media.name: " << (media_name ? media_name : "null") << "\n";
        std::cout << "  application.name: " << (application_name ? application_name : "null") << "\n";
        std::cout << "  application.process.name: " << (application_process_name ? application_process_name : "null") << "\n";
        std::cout << "  window.title: " << (window_title ? window_title : "null") << "\n";
        std::cout << "  media.title: " << (media_title ? media_title : "null") << "\n";
        std::cout << "  media.role: " << (media_role ? media_role : "null") << "\n";
        std::cout << "  stream_name: " << (stream_name ? stream_name : "null") << "\n";
        
        // Build descriptive name with available information
        if (window_title && media_title) {
            app_name = std::string(window_title) + " - " + media_title;
        } else if (window_title) {
            app_name = window_title;
        } else if (media_title) {
            app_name = media_title;
        } else if (media_name) {
            app_name = media_name;
        } else if (application_name) {
            app_name = application_name;
        } else if (stream_name) {
            app_name = stream_name;
        }
        
        // Add process name if available and different
        if (application_process_name && app_name.find(application_process_name) == std::string::npos) {
            app_name += " (" + std::string(application_process_name) + ")";
        }
        
        ac->available_applications_[i->index] = app_name;
    }
        
    if (eol < 0) {
        std::cerr << "Error getting sink input info" << std::endl;
    }
        
    pa_threaded_mainloop_signal(static_cast<PulseAudioCapture*>(userdata)->mainloop_, 0);
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
            
    auto get_sink_cb = [](pa_context* /*c*/, const pa_sink_input_info* i, int eol, void* userdata) {
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
    
    return true;  // Return success
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