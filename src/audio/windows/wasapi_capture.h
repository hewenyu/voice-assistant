#pragma once

#include <audio/audio_capture.h>
#include <audio/audio_format.h>
#include <core/message_bus.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <string>

namespace windows_audio {

class WasapiCapture : public audio::IAudioCapture {
public:
    WasapiCapture();
    ~WasapiCapture() override;

    // IAudioCapture interface implementation
    bool initialize() override;
    bool start_recording_application(unsigned int session_id) override;
    void stop_recording() override;
    void list_applications() override;
    audio::AudioFormat get_audio_format() const override;
    void set_message_bus(std::shared_ptr<core::MessageBus> message_bus) override {
        message_bus_ = message_bus;
    }

private:
    // COM initialization helper
    class ComInitializer {
    public:
        ComInitializer();
        ~ComInitializer();
        bool IsInitialized() const { return initialized_; }
    private:
        bool initialized_;
    };
    ComInitializer com_init_;

    // COM interface pointers
    IMMDeviceEnumerator* device_enumerator_;
    IMMDevice* audio_device_;
    IAudioClient* audio_client_;
    IAudioCaptureClient* capture_client_;
    
    // Audio session control
    IAudioSessionManager2* session_manager_;
    IAudioSessionEnumerator* session_enumerator_;
    
    // Recording state
    bool is_recording_;
    std::vector<float> audio_buffer_;  // Changed to float for direct message publishing
    HANDLE capture_thread_;
    HANDLE stop_event_;

    // Audio format settings
    static constexpr DWORD SAMPLE_RATE = 16000;
    static constexpr WORD CHANNELS = 1;
    static constexpr WORD BITS_PER_SAMPLE = 16;
    
    WAVEFORMATEX* mix_format_;
    
    // Available audio sessions
    std::map<DWORD, std::wstring> available_applications_;
    
    // Helper functions
    void cleanup();
    static DWORD WINAPI CaptureThread(LPVOID param);
    void process_captured_data(const BYTE* buffer, UINT32 frames);
    HRESULT initialize_com();
    HRESULT initialize_audio_client();
    HRESULT get_default_device();
    HRESULT enumerate_audio_sessions();

    std::shared_ptr<core::MessageBus> message_bus_;
};

} // namespace windows_audio 