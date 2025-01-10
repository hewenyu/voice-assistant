#pragma once

#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include "sherpa-onnx/c-api/c-api.h"
#include <vector>
#include <mutex>
#include <memory>

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

    // 流式处理相关的结构
    struct StreamContext {
        std::vector<float> audio_buffer;              // 音频数据缓冲区
        const SherpaOnnxOfflineStream* stream;        // sherpa-onnx 流
        std::string current_text;                     // 当前识别的文本
        bool has_final_result;                        // 是否有最终结果

        StreamContext() : stream(nullptr), has_final_result(false) {}
        ~StreamContext() {
            if (stream) {
                SherpaOnnxDestroyOfflineStream(stream);
            }
        }
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
    std::mutex mutex_;  // 用于保护共享资源
}; 