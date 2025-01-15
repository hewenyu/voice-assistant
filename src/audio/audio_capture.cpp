#include "audio/audio_capture.h"
#include "audio/linux_pulease/pulse_audio_capture.h"
#ifdef _WIN32
#include "audio/windows/wasapi_capture.h"
#endif

namespace audio {

std::unique_ptr<IAudioCapture> IAudioCapture::CreateAudioCapture() {
#ifdef _WIN32
    return std::make_unique<windows::WasapiCapture>();
#else
    return std::make_unique<linux_pulse::PulseAudioCapture>();
#endif
}


} // namespace audio 