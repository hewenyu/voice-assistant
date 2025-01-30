#ifndef CORE_AUDIO_WASAPI_CAPTURE_H
#define CORE_AUDIO_WASAPI_CAPTURE_H

#include "../audio_capture.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>

namespace core {
namespace audio {

class WasapiCapture : public IAudioCapture {
public:
    WasapiCapture();
    ~WasapiCapture() override;

    bool initialize() override;
    bool start() override;
    void stop() override;
    void setCallback(std::function<void(float*, int)> callback) override;
    bool getFormat(AudioFormat& format) override;
    int getApplications(AudioAppInfo* apps, int maxCount) override;
    bool startProcess(unsigned int pid) override;

private:
    static DWORD WINAPI captureThreadProc(LPVOID param);
    DWORD captureProc();
    void cleanup();

    IMMDeviceEnumerator* deviceEnumerator_;
    IMMDevice* audioDevice_;
    IAudioClient* audioClient_;
    IAudioCaptureClient* captureClient_;
    IAudioSessionManager2* sessionManager_;
    WAVEFORMATEX* mixFormat_;
    
    bool isInitialized_;
    bool stopCapture_;
    HANDLE captureThread_;
    std::function<void(float*, int)> callback_;
};

} // namespace audio
} // namespace core

#endif // CORE_AUDIO_WASAPI_CAPTURE_H 