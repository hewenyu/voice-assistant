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

class AudioCapture {
private:
    pa_threaded_mainloop *mainloop;
    pa_context *context;
    pa_stream *stream;
    pa_sample_spec ss;
    std::string app_name;
    bool is_recording;
    std::map<std::string, std::string> available_sources;
    std::map<uint32_t, std::string> available_applications;
    
    static void context_state_cb(pa_context *c, void *userdata) {
        auto *ac = static_cast<AudioCapture*>(userdata);
        switch (pa_context_get_state(c)) {
            case PA_CONTEXT_READY:
            case PA_CONTEXT_TERMINATED:
            case PA_CONTEXT_FAILED:
                pa_threaded_mainloop_signal(ac->mainloop, 0);
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
            // Write data to file or process it
            std::cout << "Read " << length << " bytes of audio data" << std::endl;
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
        auto *mainloop = static_cast<AudioCapture*>(userdata)->mainloop;
        pa_threaded_mainloop_signal(mainloop, 0);
    }

public:
    AudioCapture() : mainloop(nullptr), context(nullptr), stream(nullptr), is_recording(false) {
        mainloop = pa_threaded_mainloop_new();
        if (!mainloop) {
            throw std::runtime_error("Failed to create mainloop");
        }
        
        if (pa_threaded_mainloop_start(mainloop) < 0) {
            pa_threaded_mainloop_free(mainloop);
            throw std::runtime_error("Failed to start mainloop");
        }
        
        pa_threaded_mainloop_lock(mainloop);
        
        context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "AudioCapture");
        if (!context) {
            pa_threaded_mainloop_unlock(mainloop);
            pa_threaded_mainloop_stop(mainloop);
            pa_threaded_mainloop_free(mainloop);
            throw std::runtime_error("Failed to create context");
        }
        
        pa_context_set_state_callback(context, context_state_cb, this);
        
        if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
            pa_threaded_mainloop_unlock(mainloop);
            pa_context_unref(context);
            pa_threaded_mainloop_stop(mainloop);
            pa_threaded_mainloop_free(mainloop);
            throw std::runtime_error("Failed to connect context");
        }
        
        // Wait for context to be ready
        while (true) {
            pa_context_state_t state = pa_context_get_state(context);
            if (state == PA_CONTEXT_READY) {
                break;
            }
            if (!PA_CONTEXT_IS_GOOD(state)) {
                pa_threaded_mainloop_unlock(mainloop);
                pa_context_unref(context);
                pa_threaded_mainloop_stop(mainloop);
                pa_threaded_mainloop_free(mainloop);
                throw std::runtime_error("Failed to connect to PulseAudio server");
            }
            pa_threaded_mainloop_wait(mainloop);
        }
        
        pa_threaded_mainloop_unlock(mainloop);
    }
    
    ~AudioCapture() {
        if (stream) {
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);
        }
        if (context) {
            pa_context_disconnect(context);
            pa_context_unref(context);
        }
        if (mainloop) {
            pa_threaded_mainloop_stop(mainloop);
            pa_threaded_mainloop_free(mainloop);
        }
    }
    
    void list_applications() {
        if (!context || pa_context_get_state(context) != PA_CONTEXT_READY) {
            throw std::runtime_error("PulseAudio context not ready");
        }

        available_applications.clear();
        
        pa_threaded_mainloop_lock(mainloop);
        
        pa_operation *op = pa_context_get_sink_input_info_list(context, sink_input_info_cb, this);
        if (!op) {
            pa_threaded_mainloop_unlock(mainloop);
            throw std::runtime_error("Failed to get sink input info list");
        }
        
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(mainloop);
        }
        
        pa_operation_unref(op);
        pa_threaded_mainloop_unlock(mainloop);
        
        if (available_applications.empty()) {
            std::cout << "No applications are currently playing audio." << std::endl;
        } else {
            std::cout << "Applications currently playing audio:" << std::endl;
            for (const auto& app : available_applications) {
                std::cout << "  " << app.first << ": " << app.second << std::endl;
            }
        }
    }
    
    void start_recording_application(uint32_t sink_input_index) {
        if (stream) {
            throw std::runtime_error("Already recording");
        }
        
        pa_sample_spec spec;
        spec.format = PA_SAMPLE_S16LE;
        spec.channels = 2;
        spec.rate = 44100;
        
        stream = pa_stream_new(context, "RecordStream", &spec, nullptr);
        if (!stream) {
            throw std::runtime_error("Failed to create stream");
        }
        
        pa_stream_set_state_callback(stream, stream_state_cb, mainloop);
        pa_stream_set_read_callback(stream, stream_read_cb, this);
        
        pa_buffer_attr buffer_attr;
        buffer_attr.maxlength = (uint32_t)-1;
        buffer_attr.fragsize = 4096;
        
        if (pa_stream_connect_record(stream, nullptr, &buffer_attr, 
            static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE)) < 0) {
            pa_stream_unref(stream);
            stream = nullptr;
            throw std::runtime_error("Failed to connect stream");
        }
        
        is_recording = true;
    }
    
    void stop_recording() {
        is_recording = false;
        if (stream) {
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);
            stream = nullptr;
        }
    }
    
    const std::map<uint32_t, std::string>& get_available_applications() const {
        return available_applications;
    }
}; 