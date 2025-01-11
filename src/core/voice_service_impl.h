#pragma once

#include <string>
#include <cstring>
#include "sherpa-onnx/c-api/c-api.h"
#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include <vector>
#include <mutex>
#include <memory>
#include "core/model_config.h"

class VoiceServiceImpl final : public VoiceService::Service {
public:
    explicit VoiceServiceImpl(const ModelConfig& config);
    ~VoiceServiceImpl();

    grpc::Status SyncRecognize(grpc::ServerContext* context,
                              const SyncRecognizeRequest* request,
                              SyncRecognizeResponse* response) override;

    grpc::Status AsyncRecognize(grpc::ServerContext* context,
                               const AsyncRecognizeRequest* request,
                               AsyncRecognizeResponse* response) override;

    grpc::Status GetAsyncRecognizeStatus(grpc::ServerContext* context,
                                        const GetAsyncRecognizeStatusRequest* request,
                                        GetAsyncRecognizeStatusResponse* response) override;

    grpc::Status StreamingRecognize(grpc::ServerContext* context,
                                   grpc::ServerReaderWriter<StreamingRecognizeResponse,
                                   StreamingRecognizeRequest>* stream) override;

private:
    bool InitializeRecognizer();
    bool InitializeVAD();
    std::string ProcessAudio(const std::string& audio_data);

    // 流式处理相关的结构
    struct StreamContext {
        const SherpaOnnxOfflineStream* stream = nullptr;
        std::string current_text;
        bool has_speech = false;
        bool was_speech = false;  // Track previous speech state
        int continuous_silence_chunks = 0;
    };

    // 流式处理相关的方法
    void ProcessStreamingAudio(
        StreamContext& context,
        const std::string& audio_data,
        StreamingRecognizeResponse* response
    );

    bool ProcessStreamingResult(
        StreamContext& context,
        StreamingRecognizeResponse* response
    );

    const SherpaOnnxOfflineRecognizer* recognizer_;
    SherpaOnnxOfflineRecognizerConfig config_;
    ModelConfig model_config_;  // Store the model configuration
    std::mutex mutex_;  // 用于保护共享资源

    // VAD related members
    SherpaOnnxVoiceActivityDetector* vad_;
    SherpaOnnxVadModelConfig vad_config_;
}; 