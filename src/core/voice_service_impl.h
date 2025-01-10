#pragma once

#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include "sherpa-onnx/c-api/c-api.h"

class VoiceServiceImpl final : public VoiceService::Service {
public:
    VoiceServiceImpl();
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
    std::string ProcessAudio(const std::string& audio_data);

    const SherpaOnnxOfflineRecognizer* recognizer_;
    SherpaOnnxOfflineRecognizerConfig config_;
}; 