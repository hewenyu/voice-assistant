#pragma once

#include <audio/audio_capture.h>
#include <audio/audio_format.h>
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <vector>
#include <map>
#include <mutex>
#include <string>

namespace windows_wasapi {

class WasapiCapture : public audio::IAudioCapture {
public:
    WasapiCapture();
    ~WasapiCapture() override;

    // IAudioCapture interface implementation
    bool initialize() override;
    bool start_recording_application(unsigned int pid) override;
    void stop_recording() override;
    void list_applications() override;
    void set_model_recognizer(const SherpaOnnxOfflineRecognizer* recognizer) override;
    void set_model_vad(SherpaOnnxVoiceActivityDetector* vad, const int window_size) override;
    void set_translate(const translator::ITranslator* translate) override;

private:
    // WASAPI specific members
    IMMDeviceEnumerator* device_enumerator_;
    IMMDevice* audio_device_;
    IAudioClient* audio_client_;
    IAudioCaptureClient* capture_client_;
    WAVEFORMATEX* wave_format_;
    
    // Recognition related members
    const SherpaOnnxOfflineRecognizer* recognizer_;
    const SherpaOnnxOfflineStream* recognition_stream_;
    SherpaOnnxVoiceActivityDetector* vad_;
    const translator::ITranslator* translate_;
    int window_size_;
    bool recognition_enabled_;
    
    // Audio processing members
    std::vector<float> remaining_samples_;
    std::vector<int16_t> audio_buffer;
    bool is_recording;
    audio::AudioFormat format_;
    std::mutex recognition_mutex_;
    std::map<unsigned int, std::string> available_applications_;

    // Helper methods
    void cleanup();
    void process_audio_for_recognition(const std::vector<int16_t>& audio_data);
    
    // Thread handling
    static DWORD WINAPI capture_thread(LPVOID param);
    HANDLE capture_thread_handle_;
    bool capture_thread_running_;
};

} // namespace windows_wasapi 