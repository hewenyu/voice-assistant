#include "wasapi_capture.h"
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <vector>

namespace core {
namespace audio {

// 定义 GUID
DEFINE_GUID(IID_IAudioSessionManager2, 0x77aa99a0, 0x1bd6, 0x484f, 0x8b, 0xc7, 0x2c, 0x65, 0x4c, 0x9a, 0x9b, 0x6f);

// 定义常量
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

WasapiCapture::WasapiCapture() :
    deviceEnumerator_(nullptr),
    audioDevice_(nullptr),
    audioClient_(nullptr),
    captureClient_(nullptr),
    sessionManager_(nullptr),
    mixFormat_(nullptr),
    isInitialized_(false),
    stopCapture_(false),
    captureThread_(nullptr) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

WasapiCapture::~WasapiCapture() {
    cleanup();
    CoUninitialize();
}

bool WasapiCapture::initialize() {
    if (isInitialized_) return true;

    // 创建设备枚举器
    HRESULT hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, nullptr,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&deviceEnumerator_
    );
    if (FAILED(hr)) return false;

    // 获取默认音频设备
    hr = deviceEnumerator_->GetDefaultAudioEndpoint(
        eRender, eConsole, &audioDevice_
    );
    if (FAILED(hr)) return false;

    // 激活音频客户端
    hr = audioDevice_->Activate(
        IID_IAudioClient, CLSCTX_ALL,
        nullptr, (void**)&audioClient_
    );
    if (FAILED(hr)) return false;

    // 获取混音格式
    hr = audioClient_->GetMixFormat(&mixFormat_);
    if (FAILED(hr)) return false;

    // 初始化音频客户端
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, mixFormat_, nullptr
    );
    if (FAILED(hr)) return false;

    // 获取捕获客户端
    hr = audioClient_->GetService(
        IID_IAudioCaptureClient,
        (void**)&captureClient_
    );
    if (FAILED(hr)) return false;

    isInitialized_ = true;
    return true;
}

bool WasapiCapture::start() {
    if (!isInitialized_) return false;
    
    HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) return false;

    stopCapture_ = false;
    captureThread_ = CreateThread(
        nullptr, 0,
        captureThreadProc,
        this, 0, nullptr
    );
    
    return captureThread_ != nullptr;
}

void WasapiCapture::stop() {
    if (captureThread_) {
        stopCapture_ = true;
        WaitForSingleObject(captureThread_, INFINITE);
        CloseHandle(captureThread_);
        captureThread_ = nullptr;
    }
    if (audioClient_) {
        audioClient_->Stop();
    }
}

void WasapiCapture::setCallback(std::function<void(float*, int)> callback) {
    callback_ = std::move(callback);
}

bool WasapiCapture::getFormat(AudioFormat& format) {
    if (!mixFormat_) return false;
    
    // 返回目标格式（16kHz，单声道，16位）
    format.sample_rate = 16000;
    format.channels = 1;
    format.bits_per_sample = 16;
    
    return true;
}

int WasapiCapture::getApplications(AudioAppInfo* apps, int maxCount) {
    if (!sessionManager_) {
        HRESULT hr = audioDevice_->Activate(
            __uuidof(IAudioSessionManager2),
            CLSCTX_ALL,
            nullptr,
            (void**)&sessionManager_
        );
        if (FAILED(hr)) return 0;
    }

    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    HRESULT hr = sessionManager_->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr)) return 0;

    int count = 0;
    int sessionCount = 0;
    sessionEnumerator->GetCount(&sessionCount);

    for (int i = 0; i < sessionCount && count < maxCount; i++) {
        IAudioSessionControl* sessionControl = nullptr;
        hr = sessionEnumerator->GetSession(i, &sessionControl);
        if (FAILED(hr)) continue;

        IAudioSessionControl2* sessionControl2 = nullptr;
        hr = sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);
        sessionControl->Release();
        if (FAILED(hr)) continue;

        DWORD processId;
        hr = sessionControl2->GetProcessId(&processId);
        if (SUCCEEDED(hr) && processId != 0) {
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
            if (process) {
                apps[count].pid = processId;
                DWORD size = 260;
                if (QueryFullProcessImageNameW(process, 0, apps[count].name, &size)) {
                    count++;
                }
                CloseHandle(process);
            }
        }
        sessionControl2->Release();
    }

    sessionEnumerator->Release();
    return count;
}

bool WasapiCapture::startProcess(unsigned int pid) {
    if (!sessionManager_) {
        HRESULT hr = audioDevice_->Activate(
            __uuidof(IAudioSessionManager2),
            CLSCTX_ALL,
            nullptr,
            (void**)&sessionManager_
        );
        if (FAILED(hr)) return false;
    }

    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    HRESULT hr = sessionManager_->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr)) return false;

    int sessionCount = 0;
    sessionEnumerator->GetCount(&sessionCount);

    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl* sessionControl = nullptr;
        hr = sessionEnumerator->GetSession(i, &sessionControl);
        if (FAILED(hr)) continue;

        IAudioSessionControl2* sessionControl2 = nullptr;
        hr = sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);
        sessionControl->Release();
        if (FAILED(hr)) continue;

        DWORD processId;
        hr = sessionControl2->GetProcessId(&processId);
        if (SUCCEEDED(hr) && processId == pid) {
            sessionControl2->Release();
            sessionEnumerator->Release();
            return start();
        }
        sessionControl2->Release();
    }

    sessionEnumerator->Release();
    return false;
}

DWORD WINAPI WasapiCapture::captureThreadProc(LPVOID param) {
    auto* capture = static_cast<WasapiCapture*>(param);
    return capture->captureProc();
}

DWORD WasapiCapture::captureProc() {
    float* resampleBuffer = nullptr;
    size_t resampleBufferSize = 0;

    while (!stopCapture_) {
        UINT32 packetLength = 0;
        HRESULT hr = captureClient_->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        if (packetLength == 0) {
            Sleep(10);
            continue;
        }

        BYTE* data;
        UINT32 frames;
        DWORD flags;

        hr = captureClient_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr)) break;

        if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && callback_) {
            float* audioData = (float*)data;
            int channels = mixFormat_->nChannels;
            int originalSampleRate = mixFormat_->nSamplesPerSec;
            
            // 计算重采样后的帧数
            int resampledFrames = (int)((float)frames * 16000 / originalSampleRate);
            
            // 确保重采样缓冲区足够大
            if (resampleBufferSize < resampledFrames) {
                delete[] resampleBuffer;
                resampleBufferSize = resampledFrames;
                resampleBuffer = new float[resampleBufferSize];
            }

            // 首先转换为单声道
            float* monoData = new float[frames];
            for (UINT32 i = 0; i < frames; i++) {
                float sum = 0;
                for (int ch = 0; ch < channels; ch++) {
                    sum += audioData[i * channels + ch];
                }
                monoData[i] = sum / channels;
            }

            // 线性插值重采样到16kHz
            for (int i = 0; i < resampledFrames; i++) {
                float position = (float)i * originalSampleRate / 16000;
                int index = (int)position;
                float fraction = position - index;

                if (index >= frames - 1) {
                    resampleBuffer[i] = monoData[frames - 1];
                } else {
                    resampleBuffer[i] = monoData[index] * (1 - fraction) + 
                                      monoData[index + 1] * fraction;
                }
            }

            // 调用回调函数
            callback_(resampleBuffer, resampledFrames);
            
            delete[] monoData;
        }

        hr = captureClient_->ReleaseBuffer(frames);
        if (FAILED(hr)) break;
    }

    delete[] resampleBuffer;
    return 0;
}

void WasapiCapture::cleanup() {
    stop();
    
    if (captureClient_) {
        captureClient_->Release();
        captureClient_ = nullptr;
    }
    if (audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }
    if (audioDevice_) {
        audioDevice_->Release();
        audioDevice_ = nullptr;
    }
    if (deviceEnumerator_) {
        deviceEnumerator_->Release();
        deviceEnumerator_ = nullptr;
    }
    if (sessionManager_) {
        sessionManager_->Release();
        sessionManager_ = nullptr;
    }
    if (mixFormat_) {
        CoTaskMemFree(mixFormat_);
        mixFormat_ = nullptr;
    }
    
    isInitialized_ = false;
}

// 工厂函数实现
std::unique_ptr<IAudioCapture> createAudioCapture() {
    return std::make_unique<WasapiCapture>();
}

} // namespace audio
} // namespace core 