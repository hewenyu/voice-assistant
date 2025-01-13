#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <chrono>

class AudioCapture {
private:
    pa_threaded_mainloop *mainloop;
    pa_context *context;
    pa_stream *stream;
    pa_sample_spec ss;
    std::string app_name;
    bool is_recording;
    
    static void context_state_cb(pa_context *c, void *userdata) {
        auto *data = static_cast<AudioCapture*>(userdata);
        switch (pa_context_get_state(c)) {
            case PA_CONTEXT_READY:
            case PA_CONTEXT_TERMINATED:
            case PA_CONTEXT_FAILED:
                pa_threaded_mainloop_signal(data->mainloop, 0);
                break;
            default:
                break;
        }
    }

    static void stream_read_cb(pa_stream *p, size_t nbytes, void *userdata) {
        auto *data = static_cast<AudioCapture*>(userdata);
        const void *buffer;
        size_t bytes;

        if (pa_stream_peek(p, &buffer, &bytes) < 0) {
            std::cerr << "Failed to peek stream data" << std::endl;
            return;
        }

        if (bytes > 0 && buffer) {
            // Copy data to our buffer
            std::vector<uint8_t> temp(static_cast<const uint8_t*>(buffer), 
                                    static_cast<const uint8_t*>(buffer) + bytes);
            data->buffer = std::move(temp);
        }

        pa_stream_drop(p);
    }

public:
    std::vector<uint8_t> buffer;

    AudioCapture(const std::string& target_app) 
        : app_name(target_app), is_recording(false), 
          mainloop(nullptr), context(nullptr), stream(nullptr) {
        
        // Set up audio format
        ss.format = PA_SAMPLE_S16LE;  // 16-bit little-endian
        ss.channels = 2;              // Stereo
        ss.rate = 44100;             // 44.1kHz sampling rate
    }
    
    bool start() {
        mainloop = pa_threaded_mainloop_new();
        if (!mainloop) {
            std::cerr << "Failed to create mainloop" << std::endl;
            return false;
        }

        pa_threaded_mainloop_start(mainloop);

        pa_threaded_mainloop_lock(mainloop);
        
        context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "AudioRecorder");
        if (!context) {
            std::cerr << "Failed to create context" << std::endl;
            pa_threaded_mainloop_unlock(mainloop);
            return false;
        }

        pa_context_set_state_callback(context, context_state_cb, this);
        
        if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
            std::cerr << "Failed to connect context" << std::endl;
            pa_threaded_mainloop_unlock(mainloop);
            return false;
        }

        // Wait for context to be ready
        while (pa_context_get_state(context) != PA_CONTEXT_READY) {
            pa_threaded_mainloop_wait(mainloop);
        }

        // Create stream for recording
        pa_stream_flags_t flags = static_cast<pa_stream_flags_t>(
            PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING);

        stream = pa_stream_new(context, "AudioRecorder", &ss, nullptr);
        if (!stream) {
            std::cerr << "Failed to create stream" << std::endl;
            pa_threaded_mainloop_unlock(mainloop);
            return false;
        }

        pa_stream_set_read_callback(stream, stream_read_cb, this);

        // Connect stream to the default monitor (output) of the specified app
        if (pa_stream_connect_record(stream, app_name.c_str(), nullptr, flags) < 0) {
            std::cerr << "Failed to connect stream" << std::endl;
            pa_threaded_mainloop_unlock(mainloop);
            return false;
        }

        pa_threaded_mainloop_unlock(mainloop);
        is_recording = true;
        return true;
    }
    
    std::vector<uint8_t> capture_audio(size_t bytes_to_capture) {
        if (!is_recording || !stream) {
            return std::vector<uint8_t>();
        }

        pa_threaded_mainloop_lock(mainloop);
        
        // Wait for data
        while (buffer.empty()) {
            pa_threaded_mainloop_wait(mainloop);
        }

        // Get the data
        std::vector<uint8_t> result = std::move(buffer);
        buffer.clear();

        pa_threaded_mainloop_unlock(mainloop);
        return result;
    }
    
    void stop() {
        is_recording = false;

        if (mainloop) {
            pa_threaded_mainloop_lock(mainloop);
        }

        if (stream) {
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);
            stream = nullptr;
        }

        if (context) {
            pa_context_disconnect(context);
            pa_context_unref(context);
            context = nullptr;
        }

        if (mainloop) {
            pa_threaded_mainloop_unlock(mainloop);
            pa_threaded_mainloop_stop(mainloop);
            pa_threaded_mainloop_free(mainloop);
            mainloop = nullptr;
        }
    }
    
    ~AudioCapture() {
        stop();
    }
}; 