#pragma once

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <audio/audio_capture.h>
#include <audio/audio_format.h>
#include "sherpa-onnx/c-api/c-api.h"
#include "translator/translator.h"

namespace windows_audio {

class WasapiCapture : public audio::IAudioCapture {
public:
    explicit WasapiCapture();
    ~WasapiCapture() override;

    // IAudioCapture interface implementation
    bool initialize() override;
    bool start_recording_application(unsigned int session_id) override;
    void stop_recording() override;
    void list_applications() override;
    void set_model_recognizer(const SherpaOnnxOfflineRecognizer* recognizer) override;
    void set_model_vad(SherpaOnnxVoiceActivityDetector* vad, const int window_size) override;
    void set_translate(const translator::ITranslator* translate) override;

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
    std::vector<int16_t> audio_buffer_;
    HANDLE capture_thread_;
    HANDLE stop_event_;

    // Audio format settings
    static constexpr int SAMPLE_RATE = 16000;  // Required by VAD
    static constexpr int CHANNELS = 1;         // Mono for speech recognition
    static constexpr int BITS_PER_SAMPLE = 16; // S16LE format
    
    WAVEFORMATEX* mix_format_;
    
    // Available audio sessions
    std::map<unsigned int, std::wstring> available_applications_;
    
    // Speech recognition members
    const SherpaOnnxOfflineRecognizer* recognizer_;
    const SherpaOnnxOfflineStream* recognition_stream_;
    SherpaOnnxVoiceActivityDetector* vad_;
    int window_size_;
    std::mutex recognition_mutex_;
    bool recognition_enabled_;
    std::vector<float> remaining_samples_;

    // Translation
    const translator::ITranslator* translate_;

    // Helper functions
    void cleanup();
    static DWORD WINAPI CaptureThread(LPVOID param);
    void process_captured_data(const BYTE* buffer, UINT32 frames);
    void process_audio_for_recognition(const std::vector<int16_t>& audio_data);
    HRESULT initialize_com();
    HRESULT initialize_audio_client();
    HRESULT get_default_device();
    HRESULT enumerate_audio_sessions();
};

} // namespace windows_audio 