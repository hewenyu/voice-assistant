#pragma once

#include "audio_capture.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <endpointvolume.h>
#include <map>
#include <memory>
#include <thread>
#include <string>
#include <audio/audio_format.h>

namespace voiwindows_voicece {

// 音频会话信息
struct AudioSessionInfo {
    std::wstring name;        // 应用程序名称
    std::wstring identifier;  // 会话标识符
    DWORD process_id;        // 进程ID
    IAudioSessionControl2* control;  // 会话控制接口
    
    AudioSessionInfo() : process_id(0), control(nullptr) {}
    ~AudioSessionInfo() {
        if (control) {
            control->Release();
        }
    }
};

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
    IAudioSessionManager2* session_manager_;
    IAudioSessionEnumerator* session_enumerator_;
    
    std::unique_ptr<WavWriter> wav_writer_;
    std::map<uint32_t, AudioSessionInfo> available_sessions_;
    
    bool is_recording_;
    std::thread capture_thread_;
    bool should_stop_;

    // Audio format settings
    AudioFormat format_;
    WAVEFORMATEX* wave_format_;
    
    // Current recording session
    IAudioSessionControl2* current_session_;
    std::wstring current_session_id_;

    // Helper functions
    void cleanup();
    void capture_thread_proc();
    bool initialize_audio_client();
    bool enumerate_applications();
    bool get_application_name(DWORD process_id, std::wstring& name);
    bool setup_session_capture(const AudioSessionInfo& session);
    
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