#pragma once

#include <memory>
#include <string>
#include "voice_service.grpc.pb.h"
#include "sherpa-onnx/c-api/c-api.h"

namespace voice {

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
                                        AsyncRecognizeResponse* response) override;

    grpc::Status StreamingRecognize(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<StreamingRecognizeResponse,
                                StreamingRecognizeRequest>* stream) override;

private:
    // 初始化识别器
    bool InitializeRecognizer();
    
    // 处理音频数据
    RecognitionResult ProcessAudio(const std::string& audio_content,
                                 const RecognitionConfig& config);

    // sherpa-onnx 相关成员
    struct SherpaOnnxOnlineRecognizer* recognizer_;
    struct SherpaOnnxOnlineStream* stream_;
    
    // 配置相关
    std::string model_dir_;
    bool initialized_;
}; 