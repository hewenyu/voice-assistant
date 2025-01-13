#include "wasapi_capture.h"
#include <iostream>
#include <psapi.h>

namespace voice {

// COM初始化器实现
WasapiCapture::ComInitializer::ComInitializer() : initialized_(false) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    initialized_ = SUCCEEDED(hr);
}

WasapiCapture::ComInitializer::~ComInitializer() {
    if (initialized_) {
        CoUninitialize();
    }
}

WasapiCapture::WasapiCapture(const std::string& config_path)
    : device_enumerator_(nullptr)
    , device_(nullptr)
    , audio_client_(nullptr)
    , capture_client_(nullptr)
    , session_manager_(nullptr)
    , session_enumerator_(nullptr)
    , is_recording_(false)
    , should_stop_(false)
    , wave_format_(nullptr)
    , current_session_(nullptr) {
    
    // 设置默认音频格式
    format_ = AudioFormat(16000, 1, 16);  // 16kHz, mono, 16-bit
}

WasapiCapture::~WasapiCapture() {
    cleanup();
}

void WasapiCapture::cleanup() {
    if (is_recording_) {
        stop_recording();
    }

    if (wave_format_) {
        CoTaskMemFree(wave_format_);
        wave_format_ = nullptr;
    }

    if (capture_client_) {
        capture_client_->Release();
        capture_client_ = nullptr;
    }

    if (audio_client_) {
        audio_client_->Release();
        audio_client_ = nullptr;
    }

    if (current_session_) {
        current_session_->Release();
        current_session_ = nullptr;
    }

    if (session_enumerator_) {
        session_enumerator_->Release();
        session_enumerator_ = nullptr;
    }

    if (session_manager_) {
        session_manager_->Release();
        session_manager_ = nullptr;
    }

    if (device_) {
        device_->Release();
        device_ = nullptr;
    }

    if (device_enumerator_) {
        device_enumerator_->Release();
        device_enumerator_ = nullptr;
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

    hr = device_enumerator_->GetDefaultAudioEndpoint(
        eRender,  // 捕获渲染端点（扬声器）
        eConsole, // 控制台设备
        &device_
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio endpoint" << std::endl;
        return false;
    }

    // 获取会话管理器
    hr = device_->Activate(
        __uuidof(IAudioSessionManager2),
        CLSCTX_ALL,
        nullptr,
        (void**)&session_manager_
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to get session manager" << std::endl;
        return false;
    }

    return enumerate_applications();
}

bool WasapiCapture::get_application_name(DWORD process_id, std::wstring& name) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (!process) {
        return false;
    }

    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (!QueryFullProcessImageNameW(process, 0, path, &size)) {
        CloseHandle(process);
        return false;
    }

    WCHAR* filename = wcsrchr(path, L'\\');
    if (filename) {
        name = filename + 1;
    } else {
        name = path;
    }

    CloseHandle(process);
    return true;
}

bool WasapiCapture::enumerate_applications() {
    available_sessions_.clear();

    // 获取会话枚举器
    HRESULT hr = session_manager_->GetSessionEnumerator(&session_enumerator_);
    if (FAILED(hr)) {
        std::cerr << "Failed to get session enumerator" << std::endl;
        return false;
    }

    int session_count;
    hr = session_enumerator_->GetCount(&session_count);
    if (FAILED(hr)) {
        std::cerr << "Failed to get session count" << std::endl;
        return false;
    }

    // 添加系统音频作为ID 0
    AudioSessionInfo system_audio;
    system_audio.name = L"System Audio";
    system_audio.identifier = L"system";
    system_audio.process_id = 0;
    available_sessions_[0] = system_audio;

    // 枚举所有音频会话
    for (int i = 0; i < session_count; i++) {
        IAudioSessionControl* session_control = nullptr;
        hr = session_enumerator_->GetSession(i, &session_control);
        if (FAILED(hr)) continue;

        IAudioSessionControl2* session_control2 = nullptr;
        hr = session_control->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&session_control2);
        session_control->Release();
        if (FAILED(hr)) continue;

        DWORD process_id;
        hr = session_control2->GetProcessId(&process_id);
        if (SUCCEEDED(hr) && process_id != 0) {
            AudioSessionInfo session;
            session.process_id = process_id;
            session.control = session_control2;

            // 获取会话标识符
            LPWSTR session_id;
            hr = session_control2->GetSessionInstanceIdentifier(&session_id);
            if (SUCCEEDED(hr)) {
                session.identifier = session_id;
                CoTaskMemFree(session_id);
            }

            // 获取应用程序名称
            if (get_application_name(process_id, session.name)) {
                available_sessions_[process_id] = std::move(session);
            } else {
                session_control2->Release();
            }
        } else {
            session_control2->Release();
        }
    }

    return true;
}

void WasapiCapture::list_applications() {
    enumerate_applications();
    std::cout << "Available audio sources:" << std::endl;
    for (const auto& session : available_sessions_) {
        std::wcout << "ID: " << session.first 
                  << ", Name: " << session.second.name 
                  << std::endl;
    }
}

bool WasapiCapture::setup_session_capture(const AudioSessionInfo& session) {
    HRESULT hr;

    // 如果是系统音频
    if (session.process_id == 0) {
        hr = device_->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            (void**)&audio_client_
        );
    } else {
        // 获取会话音频客户端
        IAudioSessionControl2* control = session.control;
        if (!control) return false;

        hr = device_->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            (void**)&audio_client_
        );
    }

    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client" << std::endl;
        return false;
    }

    hr = audio_client_->GetMixFormat(&wave_format_);
    if (FAILED(hr)) {
        std::cerr << "Failed to get mix format" << std::endl;
        return false;
    }

    // 配置音频客户端
    REFERENCE_TIME requested_duration = 10000000;  // 1秒 = 10,000,000 百纳秒
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        session.process_id == 0 ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
        requested_duration,
        0,
        wave_format_,
        nullptr
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client" << std::endl;
        return false;
    }

    hr = audio_client_->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&capture_client_
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to get capture client" << std::endl;
        return false;
    }

    return true;
}

bool WasapiCapture::start_recording_application(uint32_t app_id,
                                              const std::string& output_path) {
    if (is_recording_) {
        std::cerr << "Already recording" << std::endl;
        return false;
    }

    auto it = available_sessions_.find(app_id);
    if (it == available_sessions_.end()) {
        std::cerr << "Application ID not found" << std::endl;
        return false;
    }

    if (!setup_session_capture(it->second)) {
        std::cerr << "Failed to setup session capture" << std::endl;
        return false;
    }

    if (!output_path.empty()) {
        wav_writer_ = std::make_unique<WavWriter>();
        AudioFormat output_format(
            wave_format_->nSamplesPerSec,
            wave_format_->nChannels,
            wave_format_->wBitsPerSample
        );
        
        if (!wav_writer_->open(output_path, output_format)) {
            std::cerr << "Failed to open output file" << std::endl;
            return false;
        }
    }

    should_stop_ = false;
    is_recording_ = true;
    capture_thread_ = std::thread(&WasapiCapture::capture_thread_proc, this);

    return true;
}

void WasapiCapture::capture_thread_proc() {
    HRESULT hr = audio_client_->Start();
    if (FAILED(hr)) {
        std::cerr << "Failed to start audio client" << std::endl;
        return;
    }

    while (!should_stop_) {
        UINT32 packet_length = 0;
        BYTE* data;
        DWORD flags;

        hr = capture_client_->GetNextPacketSize(&packet_length);
        if (FAILED(hr)) {
            std::cerr << "Failed to get next packet size" << std::endl;
            break;
        }

        if (packet_length == 0) {
            Sleep(10);  // 没有数据时等待一会
            continue;
        }

        hr = capture_client_->GetBuffer(
            &data,
            &packet_length,
            &flags,
            nullptr,
            nullptr
        );

        if (FAILED(hr)) {
            std::cerr << "Failed to get buffer" << std::endl;
            break;
        }

        if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && wav_writer_) {
            wav_writer_->write(data, packet_length * wave_format_->nBlockAlign);
        }

        hr = capture_client_->ReleaseBuffer(packet_length);
        if (FAILED(hr)) {
            std::cerr << "Failed to release buffer" << std::endl;
            break;
        }
    }

    audio_client_->Stop();
}

void WasapiCapture::stop_recording() {
    if (!is_recording_) return;

    should_stop_ = true;
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    is_recording_ = false;

    if (wav_writer_) {
        wav_writer_->close();
        wav_writer_.reset();
    }
}

} // namespace voice 