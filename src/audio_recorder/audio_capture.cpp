#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <chrono>
#include <map>
#include <fstream>
#include <core/model_config.h>
#include <core/model_factory.h>
#include "sherpa-onnx/c-api/c-api.h"
#include <mutex>

// Define output modes
enum class OutputMode {
    FILE,
    MODEL,
    BOTH
};

class AudioCapture {
private:
    pa_threaded_mainloop* mainloop_;
    pa_context* context_;
    pa_stream* stream_;
    std::string app_name;
    bool is_recording;
    std::vector<int16_t> audio_buffer;  // Buffer for audio data
    voice::ModelConfig model_config_;
    OutputMode output_mode_;
    std::ofstream output_file_;
    
    // Speech recognition members
    const SherpaOnnxOfflineRecognizer* recognizer_;
    const SherpaOnnxOfflineStream* recognition_stream_;
    SherpaOnnxVoiceActivityDetector* vad_;
    std::mutex recognition_mutex_;
    bool recognition_enabled_;
    
    std::map<std::string, std::string> available_sources;
    std::map<uint32_t, std::string> available_applications;

    // Audio format settings for speech recognition
    static constexpr int SAMPLE_RATE = 16000;  // Required by VAD
    static constexpr int CHANNELS = 1;         // Mono for speech recognition
    static constexpr int BITS_PER_SAMPLE = 16; // S16LE format
    
    // Resampling state
    pa_sample_spec source_spec;
    pa_sample_spec target_spec;
    
    // Initialize speech recognition
    bool initialize_recognition() {
        if (!model_config_.vad.model_path.empty()) {
            // Initialize VAD
            SherpaOnnxVadModelConfig vad_config = {};
            vad_config.silero_vad.model = model_config_.vad.model_path.c_str();
            vad_config.silero_vad.threshold = model_config_.vad.threshold;
            vad_config.silero_vad.min_silence_duration = model_config_.vad.min_silence_duration;
            vad_config.silero_vad.min_speech_duration = model_config_.vad.min_speech_duration;
            vad_config.silero_vad.max_speech_duration = model_config_.vad.max_speech_duration;
            vad_config.silero_vad.window_size = model_config_.vad.window_size;
            vad_config.sample_rate = SAMPLE_RATE;
            vad_config.num_threads = model_config_.vad.num_threads;
            vad_config.debug = model_config_.vad.debug ? 1 : 0;

            vad_ = SherpaOnnxCreateVoiceActivityDetector(&vad_config, 30);
            if (!vad_) {
                std::cerr << "Failed to create VAD" << std::endl;
                return false;
            }
        }

        // Initialize recognizer
        try {
            recognizer_ = voice::ModelFactory::CreateModel(model_config_);
            if (!recognizer_) {
                std::cerr << "Failed to create recognizer" << std::endl;
                return false;
            }

            recognition_stream_ = SherpaOnnxCreateOfflineStream(recognizer_);
            if (!recognition_stream_) {
                std::cerr << "Failed to create recognition stream" << std::endl;
                return false;
            }

            recognition_enabled_ = true;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize recognition: " << e.what() << std::endl;
            return false;
        }
    }

    // Process audio data for recognition
    void process_audio_for_recognition(const std::vector<int16_t>& audio_data) {
        if (!recognition_enabled_ || !recognizer_ || !recognition_stream_) {
            return;
        }

        std::lock_guard<std::mutex> lock(recognition_mutex_);

        // Convert to float samples
        std::vector<float> float_samples(audio_data.size());
        for (size_t i = 0; i < audio_data.size(); ++i) {
            float_samples[i] = audio_data[i] / 32768.0f;
        }

        // Check for speech if VAD is enabled
        if (vad_) {
            SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad_, float_samples.data(), static_cast<int32_t>(float_samples.size()));
            if (SherpaOnnxVoiceActivityDetectorDetected(vad_)) {
                // Process audio for recognition
                SherpaOnnxAcceptWaveformOffline(recognition_stream_, 44100, float_samples.data(), static_cast<int32_t>(float_samples.size()));
                SherpaOnnxDecodeOfflineStream(recognizer_, recognition_stream_);
                const auto* result = SherpaOnnxGetOfflineStreamResult(recognition_stream_);
                if (result && result->text) {
                    std::cout << "Recognized: " << result->text << std::endl;
                }
            }
        }
    }

    static void context_state_cb(pa_context *c, void *userdata) {
        auto *ac = static_cast<AudioCapture*>(userdata);
        switch (pa_context_get_state(c)) {
            case PA_CONTEXT_READY:
            case PA_CONTEXT_TERMINATED:
            case PA_CONTEXT_FAILED:
                pa_threaded_mainloop_signal(ac->mainloop_, 0);
                break;
            default:
                break;
        }
    }
    
    static void stream_state_cb(pa_stream *s, void *userdata) {
        auto *mainloop = static_cast<pa_threaded_mainloop*>(userdata);
        pa_threaded_mainloop_signal(mainloop, 0);
    }
    
    static void stream_read_cb(pa_stream *s, size_t length, void *userdata) {
        auto *ac = static_cast<AudioCapture*>(userdata);
        const void *data;
        
        if (pa_stream_peek(s, &data, &length) < 0) {
            std::cerr << "Failed to read from stream" << std::endl;
            return;
        }
        
        if (data && length > 0 && ac->is_recording) {
            // Convert audio data to the required format (16kHz, mono, S16LE)
            const int16_t *samples = static_cast<const int16_t*>(data);
            size_t num_samples = length / sizeof(int16_t);
            
            // If stereo, convert to mono by averaging channels
            if (ac->source_spec.channels == 2) {
                for (size_t i = 0; i < num_samples; i += 2) {
                    int32_t mono_sample = (static_cast<int32_t>(samples[i]) + 
                                         static_cast<int32_t>(samples[i + 1])) / 2;
                    ac->audio_buffer.push_back(static_cast<int16_t>(mono_sample));
                }
            } else {
                ac->audio_buffer.insert(ac->audio_buffer.end(), samples, samples + num_samples);
            }
            
            // Resample if needed (simple linear resampling)
            if (ac->source_spec.rate != SAMPLE_RATE) {
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
            
            // Handle output based on mode
            if (ac->output_mode_ == OutputMode::FILE || ac->output_mode_ == OutputMode::BOTH) {
                if (ac->output_file_.is_open()) {
                    ac->output_file_.write(reinterpret_cast<const char*>(ac->audio_buffer.data()), 
                                         ac->audio_buffer.size() * sizeof(int16_t));
                }
            }
            
            if (ac->output_mode_ == OutputMode::MODEL || ac->output_mode_ == OutputMode::BOTH) {
                if (ac->recognition_enabled_) {
                    ac->process_audio_for_recognition(ac->audio_buffer);
                }
            }
            
            std::cout << "Processed " << ac->audio_buffer.size() << " samples" << std::endl;
        }
        
        pa_stream_drop(s);
    }
    
    static void sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
        if (!eol && i) {
            auto *ac = static_cast<AudioCapture*>(userdata);
            
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
            
            ac->available_applications[i->index] = app_name;
        }
        auto *mainloop = static_cast<AudioCapture*>(userdata)->mainloop_;
        pa_threaded_mainloop_signal(mainloop, 0);
    }

public:
    AudioCapture(const std::string& config_path = "", OutputMode mode = OutputMode::FILE) 
        : mainloop_(nullptr), context_(nullptr), stream_(nullptr), is_recording(false),
          recognizer_(nullptr), recognition_stream_(nullptr), vad_(nullptr),
          recognition_enabled_(false), output_mode_(mode) {
        // Load model configuration if provided and needed
        if ((mode == OutputMode::MODEL || mode == OutputMode::BOTH) && !config_path.empty()) {
            model_config_ = voice::ModelConfig::LoadFromFile(config_path);
            std::string error = model_config_.Validate();
            if (!error.empty()) {
                throw std::runtime_error("Invalid model configuration: " + error);
            }
            if (!initialize_recognition()) {
                throw std::runtime_error("Failed to initialize speech recognition");
            }
        }
        
        // Initialize PulseAudio
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
    }
    
    ~AudioCapture() {
        if (recognition_stream_) {
            SherpaOnnxDestroyOfflineStream(recognition_stream_);
        }
        if (recognizer_) {
            SherpaOnnxDestroyOfflineRecognizer(recognizer_);
        }
        if (vad_) {
            SherpaOnnxDestroyVoiceActivityDetector(vad_);
        }
        
        if (stream_) {
            pa_stream_disconnect(stream_);
            pa_stream_unref(stream_);
        }
        if (context_) {
            pa_context_disconnect(context_);
            pa_context_unref(context_);
        }
        if (mainloop_) {
            pa_threaded_mainloop_stop(mainloop_);
            pa_threaded_mainloop_free(mainloop_);
        }
    }
    
    void list_applications() {
        if (!context_ || pa_context_get_state(context_) != PA_CONTEXT_READY) {
            throw std::runtime_error("PulseAudio context not ready");
        }

        available_applications.clear();
        
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
        
        if (available_applications.empty()) {
            std::cout << "No applications are currently playing audio." << std::endl;
        } else {
            std::cout << "Applications currently playing audio:" << std::endl;
            for (const auto& app : available_applications) {
                std::cout << "  " << app.first << ": " << app.second << std::endl;
            }
        }
    }
    
    void start_recording_application(uint32_t sink_input_index, const std::string& output_path = "") {
        if (stream_) {
            throw std::runtime_error("Already recording");
        }

        // Open output file if in FILE or BOTH mode
        if ((output_mode_ == OutputMode::FILE || output_mode_ == OutputMode::BOTH) && !output_path.empty()) {
            output_file_.open(output_path, std::ios::binary);
            if (!output_file_.is_open()) {
                throw std::runtime_error("Failed to open output file: " + output_path);
            }
        }

        // Set up source format (we'll get this from the sink input)
        source_spec.format = PA_SAMPLE_S16LE;
        source_spec.channels = 2;  // Default to stereo
        source_spec.rate = 44100;  // Default to 44.1kHz
        
        // Set up target format for speech recognition
        target_spec.format = PA_SAMPLE_S16LE;
        target_spec.channels = CHANNELS;
        target_spec.rate = SAMPLE_RATE;
        
        // Create stream with source format (we'll convert later)
        stream_ = pa_stream_new(context_, "RecordStream", &source_spec, nullptr);
        if (!stream_) {
            throw std::runtime_error("Failed to create stream");
        }
        
        pa_stream_set_state_callback(stream_, stream_state_cb, mainloop_);
        pa_stream_set_read_callback(stream_, stream_read_cb, this);
        
        // Set up buffer attributes
        pa_buffer_attr buffer_attr;
        buffer_attr.maxlength = (uint32_t)-1;
        buffer_attr.fragsize = 4096;  // Read in small chunks
        
        // Get the sink name for this application
        pa_threaded_mainloop_lock(mainloop_);
        
        bool found = false;
        std::string sink_name;
        
        // Create a persistent pair for the callback data
        auto sink_data = std::make_pair(&found, &sink_name);
        
        // Get sink input info to find its sink
        auto get_sink_cb = [](pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
            if (!eol && i) {
                auto *data = static_cast<std::pair<bool*, std::string*>*>(userdata);
                *data->first = true;
                *data->second = i->sink;
            }
            auto *mainloop = static_cast<AudioCapture*>(
                static_cast<std::pair<AudioCapture*, std::pair<bool*, std::string*>*>*>(userdata)->first
            )->mainloop_;
            pa_threaded_mainloop_signal(mainloop, 0);
        };
        
        std::pair<AudioCapture*, std::pair<bool*, std::string*>*> cb_data = {this, &sink_data};
        pa_operation *op = pa_context_get_sink_input_info(context_, sink_input_index, get_sink_cb, &cb_data);
        
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(mainloop_);
        }
        pa_operation_unref(op);
        
        if (!found) {
            pa_threaded_mainloop_unlock(mainloop_);
            throw std::runtime_error("Failed to find sink for application");
        }
        
        // Connect to the monitor source of the sink
        std::string monitor_source = sink_name + ".monitor";
        
        if (pa_stream_connect_record(stream_, monitor_source.c_str(), &buffer_attr,
            static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE)) < 0) {
            pa_threaded_mainloop_unlock(mainloop_);
            pa_stream_unref(stream_);
            stream_ = nullptr;
            throw std::runtime_error("Failed to connect stream");
        }
        
        pa_threaded_mainloop_unlock(mainloop_);
        is_recording = true;
        
        // Clear any existing audio data
        clear_audio_data();
    }
    
    void stop_recording() {
        is_recording = false;
        if (stream_) {
            pa_stream_disconnect(stream_);
            pa_stream_unref(stream_);
            stream_ = nullptr;
        }
        if (output_file_.is_open()) {
            output_file_.close();
        }
    }
    
    const std::map<uint32_t, std::string>& get_available_applications() const {
        return available_applications;
    }
    
    // Get the recorded audio data
    std::vector<int16_t> get_audio_data() {
        return audio_buffer;
    }
    
    // Clear the audio buffer
    void clear_audio_data() {
        audio_buffer.clear();
    }
    
    // Get the model configuration
    const voice::ModelConfig& get_model_config() const {
        return model_config_;
    }
}; 