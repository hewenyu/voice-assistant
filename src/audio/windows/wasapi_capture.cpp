#include "wasapi_capture.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

// Add Windows link libraries
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace windows_wasapi {

WasapiCapture::WasapiCapture()
    : device_enumerator_(nullptr)
    , audio_device_(nullptr)
    , audio_client_(nullptr)
    , capture_client_(nullptr)
    , wave_format_(nullptr)
    , recognizer_(nullptr)
    , recognition_stream_(nullptr)
    , vad_(nullptr)
    , translate_(nullptr)
    , window_size_(0)
    , recognition_enabled_(false)
    , is_recording(false)
    , capture_thread_running_(false) {
    
    // Set default audio format (16kHz, mono, 16-bit)
    format_.sample_rate = 16000;
    format_.channels = 1;
    format_.bits_per_sample = 16;
    
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to initialize COM");
    }
}

WasapiCapture::~WasapiCapture() {
    cleanup();
    CoUninitialize();
}

void WasapiCapture::cleanup() {
    stop_recording();
    
    if (capture_client_) {
        capture_client_->Release();
        capture_client_ = nullptr;
    }
    
    if (audio_client_) {
        audio_client_->Release();
        audio_client_ = nullptr;
    }
    
    if (audio_device_) {
        audio_device_->Release();
        audio_device_ = nullptr;
    }
    
    if (device_enumerator_) {
        device_enumerator_->Release();
        device_enumerator_ = nullptr;
    }
    
    if (wave_format_) {
        CoTaskMemFree(wave_format_);
        wave_format_ = nullptr;
    }
}

bool WasapiCapture::initialize() {
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&device_enumerator_
    );
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create device enumerator" << std::endl;
        return false;
    }
    
    return true;
}

void WasapiCapture::set_model_recognizer(const SherpaOnnxOfflineRecognizer* recognizer) {
    std::lock_guard<std::mutex> lock(recognition_mutex_);
    
    if (!vad_) {
        std::cerr << "[ERROR] VAD is not initialized" << std::endl;
        throw std::runtime_error("VAD is not initialized");
    }
    
    recognizer_ = recognizer;
    if (!recognizer_) {
        std::cerr << "[ERROR] Recognizer is not initialized" << std::endl;
        throw std::runtime_error("Recognizer is not initialized");
    }
    
    recognition_stream_ = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!recognition_stream_) {
        std::cerr << "[ERROR] Failed to create recognition stream" << std::endl;
        throw std::runtime_error("Failed to create recognition stream");
    }
    
    recognition_enabled_ = true;
}

void WasapiCapture::set_model_vad(SherpaOnnxVoiceActivityDetector* vad, const int window_size) {
    std::lock_guard<std::mutex> lock(recognition_mutex_);
    vad_ = vad;
    window_size_ = window_size;
    
    if (!vad_) {
        std::cerr << "[ERROR] VAD is not initialized" << std::endl;
        throw std::runtime_error("VAD is not initialized");
    }
}

void WasapiCapture::set_translate(const translator::ITranslator* translate) {
    translate_ = translate;
}

void WasapiCapture::process_audio_for_recognition(const std::vector<int16_t>& audio_data) {
    if (!recognition_enabled_ || !vad_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(recognition_mutex_);
    
    // Convert to float samples
    std::vector<float> float_samples(audio_data.size());
    for (size_t i = 0; i < audio_data.size(); ++i) {
        float_samples[i] = audio_data[i] / 32768.0f;
    }

    // If we have remaining samples from last batch, prepend them
    if (!remaining_samples_.empty()) {
        float_samples.insert(float_samples.begin(),
                           remaining_samples_.begin(),
                           remaining_samples_.end());
        remaining_samples_.clear();
    }

    const int window_size = window_size_;
    size_t i = 0;
    while (i + window_size <= float_samples.size()) {
        // Feed window_size samples to VAD
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(
            vad_,
            float_samples.data() + i,
            window_size
        );
        
        // Process any complete speech segments
        while (!SherpaOnnxVoiceActivityDetectorEmpty(vad_)) {
            const SherpaOnnxSpeechSegment* segment = 
                    SherpaOnnxVoiceActivityDetectorFront(vad_);
            if (segment) {
                // Create a new stream for this segment
                const SherpaOnnxOfflineStream* stream = 
                    SherpaOnnxCreateOfflineStream(recognizer_);

                if (stream) {
                    // Process the speech segment
                    SherpaOnnxAcceptWaveformOffline(
                        stream,
                        format_.sample_rate,
                        segment->samples,
                        segment->n
                    );

                    SherpaOnnxDecodeOfflineStream(recognizer_, stream);

                    const SherpaOnnxOfflineRecognizerResult* result = 
                        SherpaOnnxGetOfflineStreamResult(stream);

                    if (result && result->text) {
                        float start = segment->start / static_cast<float>(format_.sample_rate);
                        float duration = segment->n / static_cast<float>(format_.sample_rate);
                        float end = start + duration;

                        std::cout << "\n[Recognition Result]" << std::endl;
                        std::cout << "Time: " << std::fixed << std::setprecision(3)
                                  << start << "s -- " << end << "s" << std::endl;
                        std::cout << "Text: " << result->text << std::endl;

                        if (result->lang && translate_) {
                            std::string language_code = std::string(result->lang).substr(2, 2);
                            std::transform(language_code.begin(), language_code.end(), 
                                         language_code.begin(), ::toupper);
                            std::cout << "Language Code: " << language_code << std::endl;

                            std::string target_lang = translate_->get_target_language();
                            std::transform(target_lang.begin(), target_lang.end(), 
                                         target_lang.begin(), ::toupper);
                            std::cout << "Target Language: " << target_lang << std::endl;
                            
                            if (target_lang != language_code) {
                                try {
                                    std::string translated_text = 
                                        translate_->translate(result->text, language_code);
                                    std::cout << "Translated Text: " << translated_text << std::endl;
                                } catch (const std::exception& e) {
                                    std::cerr << "Error translating text: " << e.what() << std::endl;
                                }
                            }
                        }
                        std::cout << std::string(50, '-') << std::endl;
                    }

                    // Clean up
                    SherpaOnnxDestroyOfflineRecognizerResult(result);
                    SherpaOnnxDestroyOfflineStream(stream);
                }

                SherpaOnnxDestroySpeechSegment(segment);
            }
            SherpaOnnxVoiceActivityDetectorPop(vad_);
        }
        i += window_size;
    }

    // Store remaining samples for next batch
    if (i < float_samples.size()) {
        remaining_samples_.assign(
            float_samples.begin() + i,
            float_samples.end()
        );
    }
}

void WasapiCapture::list_applications() {
    available_applications_.clear();
    
    // Get the default audio endpoint
    IMMDevice* default_device = nullptr;
    HRESULT hr = device_enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &default_device);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio endpoint" << std::endl;
        return;
    }
    
    // Activate the IAudioSessionManager2 interface
    IAudioSessionManager2* session_manager = nullptr;
    hr = default_device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, 
                                nullptr, (void**)&session_manager);
    default_device->Release();
    
    if (FAILED(hr)) {
        std::cerr << "Failed to get audio session manager" << std::endl;
        return;
    }
    
    // Get the session enumerator
    IAudioSessionEnumerator* session_enumerator = nullptr;
    hr = session_manager->GetSessionEnumerator(&session_enumerator);
    session_manager->Release();
    
    if (FAILED(hr)) {
        std::cerr << "Failed to get session enumerator" << std::endl;
        return;
    }
    
    // Enumerate all sessions
    int session_count;
    hr = session_enumerator->GetCount(&session_count);
    if (SUCCEEDED(hr)) {
        std::cout << "Available audio sources:" << std::endl;
        for (int i = 0; i < session_count; i++) {
            IAudioSessionControl* session_control = nullptr;
            hr = session_enumerator->GetSession(i, &session_control);
            
            if (SUCCEEDED(hr)) {
                IAudioSessionControl2* session_control2 = nullptr;
                hr = session_control->QueryInterface(__uuidof(IAudioSessionControl2), 
                                                   (void**)&session_control2);
                session_control->Release();
                
                if (SUCCEEDED(hr)) {
                    DWORD process_id;
                    hr = session_control2->GetProcessId(&process_id);
                    
                    if (SUCCEEDED(hr) && process_id != 0) {
                        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 
                                                   FALSE, process_id);
                        if (process) {
                            wchar_t process_name[MAX_PATH];
                            DWORD size = MAX_PATH;
                            
                            if (QueryFullProcessImageNameW(process, 0, process_name, &size)) {
                                std::wstring ws(process_name);
                                std::string process_name_str(ws.begin(), ws.end());
                                size_t last_slash = process_name_str.find_last_of("\\");
                                if (last_slash != std::string::npos) {
                                    process_name_str = process_name_str.substr(last_slash + 1);
                                }
                                
                                available_applications_[process_id] = process_name_str;
                                std::cout << "  " << process_id << ": " 
                                          << process_name_str << std::endl;
                            }
                            CloseHandle(process);
                        }
                    }
                    session_control2->Release();
                }
            }
        }
    }
    session_enumerator->Release();
}

bool WasapiCapture::initialize_audio_client(unsigned int pid) {
    // Get the default audio endpoint
    IMMDevice* default_device = nullptr;
    HRESULT hr = device_enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &default_device);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio endpoint" << std::endl;
        return false;
    }
    
    // Activate the audio client
    hr = default_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 
                                nullptr, (void**)&audio_client_);
    default_device->Release();
    
    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client" << std::endl;
        return false;
    }
    
    // Get the mix format
    hr = audio_client_->GetMixFormat(&wave_format_);
    if (FAILED(hr)) {
        std::cerr << "Failed to get mix format" << std::endl;
        return false;
    }
    
    // Initialize the audio client
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0,
        0,
        wave_format_,
        nullptr
    );
    
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client" << std::endl;
        return false;
    }
    
    // Get the capture client
    hr = audio_client_->GetService(__uuidof(IAudioCaptureClient), (void**)&capture_client_);
    if (FAILED(hr)) {
        std::cerr << "Failed to get capture client" << std::endl;
        return false;
    }
    
    return true;
}

void WasapiCapture::capture_thread_proc() {
    UINT32 packet_length = 0;
    BYTE* data;
    UINT32 num_frames_available;
    DWORD flags;
    
    while (capture_thread_running_) {
        HRESULT hr = capture_client_->GetNextPacketSize(&packet_length);
        if (FAILED(hr)) {
            std::cerr << "Failed to get next packet size" << std::endl;
            break;
        }
        
        while (packet_length > 0) {
            hr = capture_client_->GetBuffer(&data, &num_frames_available, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                std::cerr << "Failed to get buffer" << std::endl;
                break;
            }
            
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                // Convert audio data to mono 16-bit samples
                std::vector<int16_t> mono_samples = convert_to_mono_16bit(
                    data, num_frames_available, wave_format_);
                
                // Resample if necessary
                if (wave_format_->nSamplesPerSec != format_.sample_rate) {
                    mono_samples = resample_to_16khz(
                        mono_samples, wave_format_->nSamplesPerSec);
                }
                
                if (recognition_enabled_) {
                    process_audio_for_recognition(mono_samples);
                }
            }
            
            hr = capture_client_->ReleaseBuffer(num_frames_available);
            if (FAILED(hr)) {
                std::cerr << "Failed to release buffer" << std::endl;
                break;
            }
            
            hr = capture_client_->GetNextPacketSize(&packet_length);
            if (FAILED(hr)) {
                std::cerr << "Failed to get next packet size" << std::endl;
                break;
            }
        }
        
        // Sleep a bit to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::vector<int16_t> WasapiCapture::convert_to_mono_16bit(
    const BYTE* data, UINT32 frames, WAVEFORMATEX* format) {
    
    std::vector<int16_t> mono_samples;
    mono_samples.reserve(frames);
    
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        const float* float_data = reinterpret_cast<const float*>(data);
        if (format->nChannels == 2) {
            for (UINT32 i = 0; i < frames * 2; i += 2) {
                float mono = (float_data[i] + float_data[i + 1]) * 0.5f;
                mono_samples.push_back(static_cast<int16_t>(mono * 32767.0f));
            }
        } else {
            for (UINT32 i = 0; i < frames; ++i) {
                mono_samples.push_back(static_cast<int16_t>(float_data[i] * 32767.0f));
            }
        }
    } else if (format->wFormatTag == WAVE_FORMAT_PCM) {
        const int16_t* pcm_data = reinterpret_cast<const int16_t*>(data);
        if (format->nChannels == 2) {
            for (UINT32 i = 0; i < frames * 2; i += 2) {
                int32_t mono = (static_cast<int32_t>(pcm_data[i]) + 
                              static_cast<int32_t>(pcm_data[i + 1])) / 2;
                mono_samples.push_back(static_cast<int16_t>(mono));
            }
        } else {
            mono_samples.assign(pcm_data, pcm_data + frames);
        }
    }
    
    return mono_samples;
}

std::vector<int16_t> WasapiCapture::resample_to_16khz(
    const std::vector<int16_t>& input, int input_rate) {
    
    std::vector<int16_t> output;
    float ratio = static_cast<float>(format_.sample_rate) / input_rate;
    size_t output_size = static_cast<size_t>(input.size() * ratio);
    output.reserve(output_size);
    
    for (size_t i = 0; i < output_size; ++i) {
        float src_idx = i / ratio;
        size_t idx1 = static_cast<size_t>(src_idx);
        size_t idx2 = idx1 + 1;
        
        if (idx2 >= input.size()) {
            output.push_back(input.back());
        } else {
            float frac = src_idx - idx1;
            int16_t sample = static_cast<int16_t>(
                input[idx1] * (1.0f - frac) + input[idx2] * frac
            );
            output.push_back(sample);
        }
    }
    
    return output;
}

bool WasapiCapture::start_recording_application(unsigned int pid) {
    if (is_recording) {
        std::cerr << "Already recording" << std::endl;
        return false;
    }
    
    if (!initialize_audio_client(pid)) {
        cleanup();
        return false;
    }
    
    // Start the audio client
    HRESULT hr = audio_client_->Start();
    if (FAILED(hr)) {
        std::cerr << "Failed to start audio client" << std::endl;
        cleanup();
        return false;
    }
    
    // Start the capture thread
    capture_thread_running_ = true;
    capture_thread_ = std::make_unique<std::thread>(&WasapiCapture::capture_thread_proc, this);
    
    is_recording = true;
    return true;
}

void WasapiCapture::stop_recording() {
    if (!is_recording) {
        return;
    }
    
    capture_thread_running_ = false;
    if (capture_thread_ && capture_thread_->joinable()) {
        capture_thread_->join();
    }
    capture_thread_.reset();
    
    if (audio_client_) {
        audio_client_->Stop();
    }
    
    cleanup();
    is_recording = false;
}

} // namespace windows_wasapi 