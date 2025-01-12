#pragma once

#include <string>
#include <cstring>
#include <map>
#include "sherpa-onnx/c-api/c-api.h"
#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include <vector>
#include <mutex>
#include <memory>
#include "core/model_config.h"

namespace voice {

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
    
    // Process audio and return recognition results
    std::vector<SpeechRecognitionResult> ProcessAudio(const std::string& audio_data, const RecognitionConfig& config);
    
    // Convert sherpa-onnx results to proto results
    void ConvertResults(const char* text, float confidence, SpeechRecognitionResult* result);

    // Async operation tracking
    struct AsyncOperation {
        std::string request_id;
        GetAsyncRecognizeStatusResponse::Status status;
        std::vector<SpeechRecognitionResult> results;
        std::string error;
    };

    // Streaming context
    struct StreamContext {
        bool is_initialized = false;
        StreamingRecognitionConfig config;
        const SherpaOnnxOfflineStream* stream = nullptr;
        std::vector<SpeechRecognitionAlternative> alternatives;
        bool has_speech = false;
        bool was_speech = false;
        int continuous_silence_chunks = 0;
        float stability = 0.0;
    };

    // Process streaming audio
    void ProcessStreamingAudio(
        StreamContext& context,
        const std::string& audio_data,
        StreamingRecognizeResponse* response
    );

    // Process streaming results
    bool ProcessStreamingResult(
        StreamContext& context,
        StreamingRecognizeResponse* response
    );

    const SherpaOnnxOfflineRecognizer* recognizer_;
    ModelConfig model_config_;
    std::mutex mutex_;

    // VAD related members
    SherpaOnnxVoiceActivityDetector* vad_;
    SherpaOnnxVadModelConfig vad_config_;

    // Async operation storage
    std::map<std::string, AsyncOperation> async_operations_;
    std::mutex async_mutex_;
};

} // namespace voice 