#include "wasapi_capture.h"
#include <iostream>
#include <functiondiscoverykeys_devpkey.h>

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
    , is_recording_(false)
    , should_stop_(false)
    , wave_format_(nullptr) {
    
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

    return initialize_audio_client();
}

bool WasapiCapture::initialize_audio_client() {
    HRESULT hr = device_->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&audio_client_
    );

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
        AUDCLNT_STREAMFLAGS_LOOPBACK,  // 捕获渲染音频
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

bool WasapiCapture::enumerate_applications() {
    // Windows不提供直接的应用程序音频流枚举
    // 这里我们只能获取默认音频设备
    available_applications_.clear();
    available_applications_[0] = "System Audio";
    return true;
}

void WasapiCapture::list_applications() {
    enumerate_applications();
    std::cout << "Available audio sources:" << std::endl;
    for (const auto& app : available_applications_) {
        std::cout << "ID: " << app.first << ", Name: " << app.second << std::endl;
    }
}

bool WasapiCapture::start_recording_application(uint32_t app_id,
                                              const std::string& output_path) {
    if (is_recording_) {
        std::cerr << "Already recording" << std::endl;
        return false;
    }

    // 在Windows中，我们只支持系统音频捕获
    if (app_id != 0) {
        std::cerr << "Only system audio capture is supported on Windows" << std::endl;
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