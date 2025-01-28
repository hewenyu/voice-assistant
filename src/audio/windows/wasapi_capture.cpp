#include "wasapi_capture.h"
#include <audio/audio_format.h>
#include <core/message_bus.h>
#include <core/message_types.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <future>
#include <psapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>

namespace windows_audio {

// COM初始化器实现
WasapiCapture::ComInitializer::ComInitializer() : initialized_(false) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    initialized_ = SUCCEEDED(hr);
}

WasapiCapture::ComInitializer::~ComInitializer() {
    if (initialized_) {
        CoUninitialize();
    }
}

WasapiCapture::WasapiCapture()
    : com_init_()
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
    , message_bus_(nullptr) {
}

WasapiCapture::~WasapiCapture() {
    cleanup();
}

audio::AudioFormat WasapiCapture::get_audio_format() const {
    return audio::AudioFormat{
        SAMPLE_RATE,
        CHANNELS,
        BITS_PER_SAMPLE
    };
}

bool WasapiCapture::initialize() {
    if (!com_init_.IsInitialized()) {
        return false;
    }

    HRESULT hr = get_default_device();
    if (FAILED(hr)) {
        return false;
    }

    hr = initialize_audio_client();
    if (FAILED(hr)) {
        return false;
    }

    stop_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    return stop_event_ != nullptr;
}

void WasapiCapture::process_captured_data(const BYTE* buffer, UINT32 frames) {
    if (!buffer || frames == 0) {
        std::cout << "No audio data received" << std::endl;
        return;
    }

    std::cout << "Processing " << frames << " frames of audio data" << std::endl;

    // 获取原始格式的参数
    int channels = mix_format_->nChannels;
    int bits_per_sample = mix_format_->wBitsPerSample;
    int bytes_per_sample = bits_per_sample / 8;
    int bytes_per_frame = bytes_per_sample * channels;

    // 转换为float格式并进行降采样
    std::vector<float> float_data;
    float_data.reserve(frames);  // 预分配空间

    // 根据位深度和通道数进行相应的转换
    if (bits_per_sample == 32) {
        const float* samples = reinterpret_cast<const float*>(buffer);
        // 只取第一个通道
        for (UINT32 i = 0; i < frames; ++i) {
            float_data.push_back(samples[i * channels]);
        }
    } else if (bits_per_sample == 16) {
        const int16_t* samples = reinterpret_cast<const int16_t*>(buffer);
        // 只取第一个通道，并转换为[-1, 1]范围
        for (UINT32 i = 0; i < frames; ++i) {
            float_data.push_back(samples[i * channels] / 32768.0f);
        }
    } else {
        std::cerr << "Unsupported bit depth: " << bits_per_sample << std::endl;
        return;
    }

    // 发布音频数据
    if (message_bus_ && !float_data.empty()) {
        std::cout << "Publishing audio data, size: " << float_data.size() << std::endl;
        auto msg = std::make_shared<core::AudioMessage>(float_data, SAMPLE_RATE, core::AudioMessage::Status::Data);
        message_bus_->publish(msg);
    } else {
        std::cout << "Message bus is not available or no data to publish" << std::endl;
    }
}

DWORD WINAPI WasapiCapture::CaptureThread(LPVOID param) {
    auto* capture = static_cast<WasapiCapture*>(param);
    
    // 创建音频事件
    HANDLE audio_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!audio_event) {
        return 1;
    }

    HANDLE events[2] = { capture->stop_event_, audio_event };

    // 设置音频事件
    HRESULT hr = capture->audio_client_->SetEventHandle(audio_event);
    if (FAILED(hr)) {
        CloseHandle(audio_event);
        return 1;
    }

    // 启动音频客户端
    hr = capture->audio_client_->Start();
    if (FAILED(hr)) {
        CloseHandle(audio_event);
        return 1;
    }

    while (true) {
        DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (wait_result == WAIT_OBJECT_0) { // Stop event
            break;
        }
        if (wait_result != WAIT_OBJECT_0 + 1) {
            break;
        }

        BYTE* data;
        UINT32 frames_available;
        DWORD flags;
        UINT64 device_position = 0;
        UINT64 qpc_position = 0;

        hr = capture->capture_client_->GetNextPacketSize(&frames_available);
        if (FAILED(hr)) {
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
                break;
            }

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                capture->process_captured_data(data, frames_available);
            }

            hr = capture->capture_client_->ReleaseBuffer(frames_available);
            if (FAILED(hr)) {
                break;
            }

            hr = capture->capture_client_->GetNextPacketSize(&frames_available);
            if (FAILED(hr)) {
                break;
            }
        }
    }

    // 停止音频客户端
    capture->audio_client_->Stop();
    CloseHandle(audio_event);
    return 0;
}

bool WasapiCapture::start_recording_application(unsigned int session_id) {
    if (is_recording_) {
        return false;
    }

    // 重新初始化音频客户端
    HRESULT hr = initialize_audio_client();
    if (FAILED(hr)) {
        return false;
    }

    is_recording_ = true;
    ResetEvent(stop_event_);

    // 发送开始录音消息
    if (message_bus_) {
        auto msg = std::make_shared<core::AudioMessage>(std::vector<float>(), SAMPLE_RATE, core::AudioMessage::Status::Started);
        message_bus_->publish(msg);
    }

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

    // 发送停止录音消息
    if (message_bus_) {
        auto msg = std::make_shared<core::AudioMessage>(std::vector<float>(), SAMPLE_RATE, core::AudioMessage::Status::Stopped);
        message_bus_->publish(msg);
    }
}

void WasapiCapture::cleanup() {
    stop_recording();

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

    // 保持原始格式，不进行修改
    // 我们将在 process_captured_data 中进行重采样和格式转换

    // 设置缓冲区时间
    const REFERENCE_TIME hns_buffer_duration = 1000000; // 100ms
    const REFERENCE_TIME hns_period = 0;  // 使用默认周期

    // 初始化音频客户端
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hns_buffer_duration,
        0,
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

} // namespace windows_audio 