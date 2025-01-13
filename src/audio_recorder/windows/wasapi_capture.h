#pragma once

#include "audio_capture.h"
#include "wav_writer.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <map>
#include <memory>
#include <thread>

namespace voice {

class WasapiCapture : public IAudioCapture {
public:
    explicit WasapiCapture(const std::string& config_path = "");
    ~WasapiCapture() override;

    // IAudioCapture interface implementation
    bool initialize() override;
    bool start_recording_application(uint32_t app_id, const std::string& output_path = "") override;
    void stop_recording() override;
    void list_applications() override;

private:
    // COM interfaces
    IMMDeviceEnumerator* device_enumerator_;
    IMMDevice* device_;
    IAudioClient* audio_client_;
    IAudioCaptureClient* capture_client_;
    
    std::unique_ptr<WavWriter> wav_writer_;
    std::map<uint32_t, std::string> available_applications_;
    
    bool is_recording_;
    std::thread capture_thread_;
    bool should_stop_;

    // Audio format settings
    AudioFormat format_;
    WAVEFORMATEX* wave_format_;

    // Helper functions
    void cleanup();
    void capture_thread_proc();
    bool initialize_audio_client();
    bool enumerate_applications();
    
    // COM initialization helper
    class ComInitializer {
    public:
        ComInitializer();
        ~ComInitializer();
    private:
        bool initialized_;
    };
    
    ComInitializer com_init_;
};

} // namespace voice 