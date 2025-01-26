#include "audio_capture.h"

#ifdef _WIN32
#include "windows/wasapi_capture.h"
#endif

#ifdef __linux__
#include "linux/pulse_audio_capture.h"
#endif

namespace audio {

std::unique_ptr<IAudioCapture> IAudioCapture::CreateAudioCapture() {
#ifdef _WIN32
    return std::make_unique<windows_audio::WasapiCapture>();
#elif __linux__
    return std::make_unique<linux_pulse::PulseAudioCapture>();
#else
    return nullptr;  // 在非Windows平台返回nullptr
#endif
}

} // namespace audio 