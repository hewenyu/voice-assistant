#include "wasapi_capture.h"
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <iomanip>
#include <psapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>

namespace windows_audio {

// COM初始化器实现
WasapiCapture::ComInitializer::ComInitializer() : initialized_(false) {
    std::cout << "Initializing COM..." << std::endl;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
        // 如果已经在另一个线程模式下初始化，尝试多线程模式
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    initialized_ = SUCCEEDED(hr);
    if (!initialized_) {
        std::cerr << "Failed to initialize COM: 0x" << std::hex << hr << std::endl;
    } else {
        std::cout << "COM initialized successfully" << std::endl;
    }
}

WasapiCapture::ComInitializer::~ComInitializer() {
    if (initialized_) {
        std::cout << "Uninitializing COM..." << std::endl;
        CoUninitialize();
    }
}

WasapiCapture::WasapiCapture()
    : com_init_()  // 确保 COM 初始化器是第一个初始化的成员
    , device_enumerator_(nullptr)
    , audio_device_(nullptr)
    , audio_client_(nullptr)
    , capture_client_(nullptr)
    , session_manager_(nullptr)
    , session_enumerator_(nullptr)
    , is_recording_(false)
    , capture_thread_(nullptr)
    , stop_event_(nullptr)
    , mix_format_(nullptr)
    , recognizer_(nullptr)
    , recognition_stream_(nullptr)
    , vad_(nullptr)
    , window_size_(0)
    , recognition_enabled_(false)
    , translate_(nullptr) {
}

WasapiCapture::~WasapiCapture() {
    cleanup();
}

bool WasapiCapture::initialize() {
    std::cout << "Initializing WasapiCapture..." << std::endl;
    
    // COM 已经由 ComInitializer 初始化，不需要再次初始化
    HRESULT hr = get_default_device();
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio device: 0x" << std::hex << hr << std::endl;
        return false;
    }

    hr = initialize_audio_client();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client: 0x" << std::hex << hr << std::endl;
        return false;
    }

    stop_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event_) {
        std::cerr << "Failed to create stop event" << std::endl;
        return false;
    }

    std::cout << "WasapiCapture initialization completed successfully" << std::endl;
    return true;
}

HRESULT WasapiCapture::get_default_device() {
    std::cout << "Getting default audio device..." << std::endl;

    if (!com_init_.IsInitialized()) {
        std::cerr << "COM is not initialized" << std::endl;
        return E_FAIL;
    }

    if (device_enumerator_) {
        std::cout << "Device enumerator already exists, releasing..." << std::endl;
        device_enumerator_->Release();
        device_enumerator_ = nullptr;
    }

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&device_enumerator_
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to create device enumerator: 0x" << std::hex << hr << std::endl;
        return hr;
    }
    std::cout << "Created device enumerator successfully" << std::endl;

    if (audio_device_) {
        std::cout << "Audio device already exists, releasing..." << std::endl;
        audio_device_->Release();
        audio_device_ = nullptr;
    }

    hr = device_enumerator_->GetDefaultAudioEndpoint(
        eRender,  // 捕获渲染设备的音频
        eConsole, // 控制台设备
        &audio_device_
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio endpoint: 0x" << std::hex << hr << std::endl;
        return hr;
    }
    std::cout << "Got default audio endpoint successfully" << std::endl;

    if (session_manager_) {
        std::cout << "Session manager already exists, releasing..." << std::endl;
        session_manager_->Release();
        session_manager_ = nullptr;
    }

    hr = audio_device_->Activate(
        __uuidof(IAudioSessionManager2),
        CLSCTX_ALL,
        nullptr,
        (void**)&session_manager_
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio session manager: 0x" << std::hex << hr << std::endl;
        return hr;
    }
    std::cout << "Activated audio session manager successfully" << std::endl;

    return S_OK;
}

HRESULT WasapiCapture::initialize_audio_client() {
    // 先释放之前的客户端
    if (capture_client_) {
        capture_client_->Release();
        capture_client_ = nullptr;
    }
    if (audio_client_) {
        audio_client_->Stop();  // 确保停止
        audio_client_->Release();
        audio_client_ = nullptr;
    }

    HRESULT hr = audio_device_->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&audio_client_
    );
    if (FAILED(hr)) return hr;

    // 获取混音格式
    hr = audio_client_->GetMixFormat(&mix_format_);
    if (FAILED(hr)) return hr;

    std::cout << "Original format - Rate: " << mix_format_->nSamplesPerSec 
              << "Hz, Channels: " << mix_format_->nChannels 
              << ", Bits: " << mix_format_->wBitsPerSample << std::endl;

    // 设置缓冲区时间
    const REFERENCE_TIME hns_buffer_duration = 1000000; // 100ms
    const REFERENCE_TIME hns_period = 0;  // 使用默认周期

    // 初始化音频客户端
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hns_buffer_duration,
        hns_period,
        mix_format_,
        nullptr
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client: 0x" << std::hex << hr << std::endl;
        return hr;
    }

    // 获取捕获客户端
    hr = audio_client_->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&capture_client_
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to get capture client: 0x" << std::hex << hr << std::endl;
        return hr;
    }

    std::cout << "Audio client initialized successfully" << std::endl;
    return S_OK;
}

void WasapiCapture::list_applications() {
    std::cout << "\nDebug: Entering list_applications()" << std::endl;
    std::cout << "Starting to list applications..." << std::endl;
    
    if (!session_manager_) {
        std::cerr << "Error: Session manager is not initialized" << std::endl;
        return;
    }
    std::cout << "Debug: Session manager is valid" << std::endl;

    HRESULT hr = enumerate_audio_sessions();
    if (FAILED(hr)) {
        std::cerr << "Failed to enumerate audio sessions: 0x" << std::hex << hr << std::endl;
        return;
    }
    std::cout << "Debug: Audio sessions enumerated successfully" << std::endl;

    if (available_applications_.empty()) {
        std::cout << "No applications are currently playing audio." << std::endl;
        return;
    }

    std::cout << "Applications currently playing audio:" << std::endl;
    for (const auto& app : available_applications_) {
        std::wcout << "  " << app.first << ": " << app.second << std::endl;
    }
    std::cout << "Debug: Exiting list_applications()" << std::endl;
}

HRESULT WasapiCapture::enumerate_audio_sessions() {
    std::cout << "Enumerating audio sessions..." << std::endl;
    available_applications_.clear();

    if (!session_manager_) {
        std::cerr << "Error: Session manager is null" << std::endl;
        return E_POINTER;
    }

    HRESULT hr = session_manager_->GetSessionEnumerator(&session_enumerator_);
    if (FAILED(hr)) {
        std::cerr << "Failed to get session enumerator: 0x" << std::hex << hr << std::endl;
        return hr;
    }

    int session_count;
    hr = session_enumerator_->GetCount(&session_count);
    if (FAILED(hr)) {
        std::cerr << "Failed to get session count: 0x" << std::hex << hr << std::endl;
        return hr;
    }

    std::cout << "Found " << session_count << " audio sessions" << std::endl;

    for (int i = 0; i < session_count; i++) {
        IAudioSessionControl* session_control = nullptr;
        hr = session_enumerator_->GetSession(i, &session_control);
        if (FAILED(hr)) {
            std::cerr << "Failed to get session " << i << ": 0x" << std::hex << hr << std::endl;
            continue;
        }

        IAudioSessionControl2* session_control2 = nullptr;
        hr = session_control->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&session_control2);
        session_control->Release();
        if (FAILED(hr)) {
            std::cerr << "Failed to get session control 2 for session " << i << ": 0x" << std::hex << hr << std::endl;
            continue;
        }

        DWORD process_id;
        hr = session_control2->GetProcessId(&process_id);
        if (FAILED(hr)) {
            // 检查是否是系统会话（Session 0）
            if (i == 0) {
                std::cout << "Skipping system audio session (Session 0)" << std::endl;
            } else {
                std::cerr << "Failed to get process ID for session " << i << ": 0x" << std::hex << hr << std::endl;
            }
            session_control2->Release();
            continue;
        }

        if (process_id == 0) {
            std::cout << "Skipping system audio session (process_id = 0)" << std::endl;
            session_control2->Release();
            continue;
        }

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
        if (process) {
            wchar_t process_name[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(process, 0, process_name, &size)) {
                available_applications_[process_id] = process_name;
                std::wcout << "Found application: " << process_name << " (PID: " << process_id << ")" << std::endl;
            } else {
                std::cerr << "Failed to get process name for PID " << process_id << std::endl;
            }
            CloseHandle(process);
        } else {
            std::cerr << "Failed to open process " << process_id << std::endl;
        }
        session_control2->Release();
    }

    return S_OK;
}

bool WasapiCapture::start_recording_application(unsigned int session_id [[maybe_unused]]) {
    if (is_recording_) {
        std::cerr << "Already recording" << std::endl;
        return false;
    }

    // 重新初始化音频客户端
    HRESULT hr = initialize_audio_client();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client: 0x" << std::hex << hr << std::endl;
        return false;
    }

    is_recording_ = true;
    ResetEvent(stop_event_);

    // Create capture thread
    capture_thread_ = CreateThread(
        nullptr,
        0,
        CaptureThread,
        this,
        0,
        nullptr
    );

    if (!capture_thread_) {
        std::cerr << "Failed to create capture thread" << std::endl;
        is_recording_ = false;
        return false;
    }

    return true;
}

void WasapiCapture::stop_recording() {
    if (!is_recording_) return;

    SetEvent(stop_event_);
    if (capture_thread_) {
        WaitForSingleObject(capture_thread_, INFINITE);
        CloseHandle(capture_thread_);
        capture_thread_ = nullptr;
    }

    if (audio_client_) {
        audio_client_->Stop();
    }

    is_recording_ = false;
}

DWORD WINAPI WasapiCapture::CaptureThread(LPVOID param) {
    auto* capture = static_cast<WasapiCapture*>(param);
    
    // 创建音频事件
    HANDLE audio_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!audio_event) {
        std::cerr << "Failed to create audio event" << std::endl;
        return 1;
    }

    HANDLE events[2] = { capture->stop_event_, audio_event };

    // 设置音频事件
    HRESULT hr = capture->audio_client_->SetEventHandle(audio_event);
    if (FAILED(hr)) {
        std::cerr << "Failed to set audio event: 0x" << std::hex << hr << std::endl;
        CloseHandle(audio_event);
        return 1;
    }

    // 启动音频客户端
    hr = capture->audio_client_->Start();
    if (FAILED(hr)) {
        std::cerr << "Failed to start audio client: 0x" << std::hex << hr << std::endl;
        CloseHandle(audio_event);
        return 1;
    }

    while (true) {
        DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (wait_result == WAIT_OBJECT_0) { // Stop event
            break;
        }
        if (wait_result != WAIT_OBJECT_0 + 1) {
            std::cerr << "Failed to wait for audio event" << std::endl;
            break;
        }

        BYTE* data;
        UINT32 frames_available;
        DWORD flags;
        UINT64 device_position = 0;
        UINT64 qpc_position = 0;

        hr = capture->capture_client_->GetNextPacketSize(&frames_available);
        if (FAILED(hr)) {
            std::cerr << "Failed to get next packet size: 0x" << std::hex << hr << std::endl;
            break;
        }

        while (frames_available > 0) {
            hr = capture->capture_client_->GetBuffer(
                &data,
                &frames_available,
                &flags,
                &device_position,
                &qpc_position
            );
            if (FAILED(hr)) {
                std::cerr << "Failed to get buffer: 0x" << std::hex << hr << std::endl;
                break;
            }

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                capture->process_captured_data(data, frames_available);
            } else {
                // 处理静音数据
                std::vector<BYTE> silence(
                    frames_available * capture->mix_format_->nBlockAlign,
                    0
                );
                capture->process_captured_data(silence.data(), frames_available);
            }

            hr = capture->capture_client_->ReleaseBuffer(frames_available);
            if (FAILED(hr)) {
                std::cerr << "Failed to release buffer: 0x" << std::hex << hr << std::endl;
                break;
            }

            hr = capture->capture_client_->GetNextPacketSize(&frames_available);
            if (FAILED(hr)) {
                std::cerr << "Failed to get next packet size: 0x" << std::hex << hr << std::endl;
                break;
            }
        }
    }

    // 停止音频客户端
    capture->audio_client_->Stop();
    CloseHandle(audio_event);
    return 0;
}

void WasapiCapture::process_captured_data(const BYTE* buffer, UINT32 frames) {
    if (!is_recording_) return;

    // 获取实际的音频格式
    WAVEFORMATEX* wfx = mix_format_;
    
    // 计算每个采样的字节数
    const int bytes_per_sample = wfx->wBitsPerSample / 8;
    const int num_channels = wfx->nChannels;
    
    // 清空临时缓冲区
    std::vector<int16_t> temp_buffer;
    temp_buffer.reserve(frames);  // 预分配空间

    // 根据音频格式进行处理
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || 
        (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && 
         reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        // 处理 float 格式
        const float* float_samples = reinterpret_cast<const float*>(buffer);
        for (UINT32 i = 0; i < frames * num_channels; i += num_channels) {
            // 将多声道混合为单声道
            float mono_sample = 0.0f;
            for (int ch = 0; ch < num_channels; ++ch) {
                mono_sample += float_samples[i + ch];
            }
            mono_sample /= num_channels;
            
            // 转换为 16-bit PCM，确保不超出范围
            int32_t pcm_value = static_cast<int32_t>(mono_sample * 32767.0f);
            pcm_value = std::min(32767, std::max(-32768, pcm_value));
            temp_buffer.push_back(static_cast<int16_t>(pcm_value));
        }
    } else {
        // 处理 PCM 格式
        const int16_t* pcm_samples = reinterpret_cast<const int16_t*>(buffer);
        for (UINT32 i = 0; i < frames * num_channels; i += num_channels) {
            // 将多声道混合为单声道
            int32_t mono_sample = 0;
            for (int ch = 0; ch < num_channels; ++ch) {
                mono_sample += static_cast<int32_t>(pcm_samples[i + ch]);
            }
            mono_sample /= num_channels;
            
            // 确保不超出范围
            mono_sample = std::min(32767, std::max(-32768, mono_sample));
            temp_buffer.push_back(static_cast<int16_t>(mono_sample));
        }
    }

    // 如果采样率不是 16kHz，进行重采样
    if (wfx->nSamplesPerSec != SAMPLE_RATE) {
        std::vector<int16_t> resampled;
        float ratio = static_cast<float>(SAMPLE_RATE) / wfx->nSamplesPerSec;
        size_t output_size = static_cast<size_t>(temp_buffer.size() * ratio);
        resampled.reserve(output_size);

        for (size_t i = 0; i < output_size; ++i) {
            float src_idx = i / ratio;
            size_t idx1 = static_cast<size_t>(src_idx);
            size_t idx2 = std::min(idx1 + 1, temp_buffer.size() - 1);
            
            float frac = src_idx - idx1;
            int32_t sample = static_cast<int32_t>(
                temp_buffer[idx1] * (1.0f - frac) + 
                temp_buffer[idx2] * frac
            );
            sample = std::min(32767, std::max(-32768, sample));
            resampled.push_back(static_cast<int16_t>(sample));
        }
        
        temp_buffer = std::move(resampled);
    }

    // 将处理后的数据添加到主缓冲区
    audio_buffer_.insert(audio_buffer_.end(), temp_buffer.begin(), temp_buffer.end());

    // Process audio for recognition if enabled
    if (recognition_enabled_) {
        process_audio_for_recognition(audio_buffer_);
        audio_buffer_.clear();
    }
}

void WasapiCapture::process_audio_for_recognition(const std::vector<int16_t>& audio_data) {
    if (!recognition_enabled_ || !vad_) return;

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

    size_t i = 0;
    while (i + window_size_ <= float_samples.size()) {
        // Feed window_size samples to VAD
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(
            vad_,
            float_samples.data() + i,
            window_size_
        );

        // Process any complete speech segments
        while (!SherpaOnnxVoiceActivityDetectorEmpty(vad_)) {
            const SherpaOnnxSpeechSegment* segment =
                SherpaOnnxVoiceActivityDetectorFront(vad_);

            if (segment) {
                // Create a new stream for this segment
                SherpaOnnxOfflineStream* stream = 
                    const_cast<SherpaOnnxOfflineStream*>(SherpaOnnxCreateOfflineStream(recognizer_));

                if (stream) {
                    // Process the speech segment
                    SherpaOnnxAcceptWaveformOffline(
                        stream,
                        SAMPLE_RATE,
                        segment->samples,
                        segment->n
                    );

                    SherpaOnnxDecodeOfflineStream(const_cast<SherpaOnnxOfflineRecognizer*>(recognizer_), stream);

                    const SherpaOnnxOfflineRecognizerResult* result = 
                        SherpaOnnxGetOfflineStreamResult(stream);

                    if (result && result->text) {
                        float start = segment->start / static_cast<float>(SAMPLE_RATE);
                        float duration = segment->n / static_cast<float>(SAMPLE_RATE);
                        float end = start + duration;

                        std::cout << "\n[Recognition Result]" << std::endl;
                        std::cout << "Time: " << std::fixed << std::setprecision(3)
                                << start << "s -- " << end << "s" << std::endl;
                        std::cout << "Text: " << result->text << std::endl;
                        // lang
                        std::cout << "Language: " << result->lang << std::endl;

                        if (result->lang && translate_) {
                            std::string language_code = std::string(result->lang).substr(2, 2);
                            std::transform(language_code.begin(), language_code.end(), language_code.begin(), ::toupper);
                            std::cout << "Language Code: " << language_code << std::endl;

                            std::string target_lang = translate_->get_target_language();
                            std::transform(target_lang.begin(), target_lang.end(), target_lang.begin(), ::toupper);
                            std::cout << "Target Language: " << target_lang << std::endl;
                            
                            if (target_lang != language_code) {
                                try {
                                    std::string translated_text = translate_->translate(result->text, language_code);
                                    std::cout << "Translated Text: " << translated_text << std::endl;
                                } catch (const std::exception& e) {
                                    std::cerr << "Error translating text: " << e.what() << std::endl;
                                }
                        }
                        std::cout << std::string(50, '-') << std::endl;
                    }

                    SherpaOnnxDestroyOfflineRecognizerResult(result);
                    SherpaOnnxDestroyOfflineStream(stream);
                }

                SherpaOnnxDestroySpeechSegment(segment);
            }
            SherpaOnnxVoiceActivityDetectorPop(vad_);
        }
        i += window_size_;
    }

    // Store remaining samples for next batch
    if (i < float_samples.size()) {
        remaining_samples_.assign(
            float_samples.begin() + i,
            float_samples.end()
        );
    }
}

void WasapiCapture::set_model_recognizer(const SherpaOnnxOfflineRecognizer* recognizer) {
    try {
        if (!vad_) {
            std::cerr << "[ERROR] VAD is not initialized" << std::endl;
            throw std::runtime_error("VAD is not initialized");
        }

        recognizer_ = recognizer;
        if (!recognizer_) {
            std::cerr << "[ERROR] Recognizer is not initialized" << std::endl;
            throw std::runtime_error("Recognizer is not initialized");
        }

        recognition_enabled_ = true;

    } catch (const std::exception& e) {
        std::cerr << "Error setting model recognizer: " << e.what() << std::endl;
        throw;
    }
}

void WasapiCapture::set_model_vad(SherpaOnnxVoiceActivityDetector* vad, const int window_size) {
    vad_ = vad;
    window_size_ = window_size;
}

void WasapiCapture::set_translate(const translator::ITranslator* translate) {
    translate_ = translate;
}

void WasapiCapture::cleanup() {
    stop_recording();

    if (recognition_stream_) {
        SherpaOnnxDestroyOfflineStream(const_cast<SherpaOnnxOfflineStream*>(recognition_stream_));
        recognition_stream_ = nullptr;
    }

    if (stop_event_) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }

    if (mix_format_) {
        CoTaskMemFree(mix_format_);
        mix_format_ = nullptr;
    }

    if (capture_client_) {
        capture_client_->Release();
        capture_client_ = nullptr;
    }

    if (audio_client_) {
        audio_client_->Stop();
        audio_client_->Release();
        audio_client_ = nullptr;
    }

    if (session_enumerator_) {
        session_enumerator_->Release();
        session_enumerator_ = nullptr;
    }

    if (session_manager_) {
        session_manager_->Release();
        session_manager_ = nullptr;
    }

    if (audio_device_) {
        audio_device_->Release();
        audio_device_ = nullptr;
    }

    if (device_enumerator_) {
        device_enumerator_->Release();
        device_enumerator_ = nullptr;
    }
    
    // 不要在这里调用 CoUninitialize，因为 com_init_ 的析构函数会处理它
}

} // namespace windows_audio 