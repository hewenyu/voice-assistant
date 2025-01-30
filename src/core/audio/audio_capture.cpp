#include "audio_capture.h"

#ifdef _WIN32
#include "windows/wasapi_capture.h"
#else
#include "linux/pulse_audio_capture.h"
#endif

namespace core::audio {

std::unique_ptr<IAudioCapture> createAudioCapture() {
#ifdef _WIN32
    return std::make_unique<WasapiCapture>();
#else
    return std::make_unique<PulseAudioCapture>();
#endif
}
} 