#include "core/voice_service_impl.h"
#include <iostream>
#include <cstring>

VoiceServiceImpl::VoiceServiceImpl() : recognizer_(nullptr) {
    InitializeRecognizer();
}

VoiceServiceImpl::~VoiceServiceImpl() {
    if (recognizer_) {
        SherpaOnnxDestroyOfflineRecognizer(recognizer_);
    }
}

bool VoiceServiceImpl::InitializeRecognizer() {
    // Configure sherpa-onnx
    memset(&config_, 0, sizeof(config_));

    // Set model paths
    SherpaOnnxOfflineSenseVoiceModelConfig sense_voice_config;
    memset(&sense_voice_config, 0, sizeof(sense_voice_config));
    sense_voice_config.model = "/home/yueban/code/github/voice-assistant/models/model.int8.onnx";
    sense_voice_config.language = "auto";
    sense_voice_config.use_itn = 1;

    // Offline model config
    SherpaOnnxOfflineModelConfig model_config;
    memset(&model_config, 0, sizeof(model_config));
    model_config.debug = 1;
    model_config.num_threads = 4;
    model_config.provider = "cpu";
    model_config.tokens = "/home/yueban/code/github/voice-assistant/models/tokens.txt";
    model_config.sense_voice = sense_voice_config;

    config_.model_config = model_config;
    config_.decoding_method = "greedy_search";

    // Create recognizer
    recognizer_ = SherpaOnnxCreateOfflineRecognizer(&config_);
    if (!recognizer_) {
        std::cerr << "Failed to create recognizer" << std::endl;
        return false;
    }

    return true;
}

std::string VoiceServiceImpl::ProcessAudio(const std::string& audio_data) {
    if (!recognizer_) {
        return "Recognizer not initialized";
    }

    // Create stream for this request
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!stream) {
        return "Failed to create stream";
    }

    // Accept audio data
    SherpaOnnxAcceptWaveformOffline(
        stream,
        16000, // sample rate
        reinterpret_cast<const float*>(audio_data.data()),
        audio_data.size() / sizeof(float)
    );

    // Decode
    SherpaOnnxDecodeOfflineStream(recognizer_, stream);

    // Get recognition result
    const SherpaOnnxOfflineRecognizerResult* result = SherpaOnnxGetOfflineStreamResult(stream);
    std::string text = result->text;

    // Clean up
    SherpaOnnxDestroyOfflineRecognizerResult(result);
    SherpaOnnxDestroyOfflineStream(stream);

    return text;
}

grpc::Status VoiceServiceImpl::SyncRecognize(grpc::ServerContext* context,
                                            const SyncRecognizeRequest* request,
                                            SyncRecognizeResponse* response) {
    std::string result = ProcessAudio(request->audio_data());
    response->set_text(result);
    return grpc::Status::OK;
}

grpc::Status VoiceServiceImpl::AsyncRecognize(grpc::ServerContext* context,
                                             const AsyncRecognizeRequest* request,
                                             AsyncRecognizeResponse* response) {
    // Not implemented yet
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Method not implemented");
}

grpc::Status VoiceServiceImpl::GetAsyncRecognizeStatus(grpc::ServerContext* context,
                                                      const GetAsyncRecognizeStatusRequest* request,
                                                      GetAsyncRecognizeStatusResponse* response) {
    // Not implemented yet
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Method not implemented");
}

grpc::Status VoiceServiceImpl::StreamingRecognize(grpc::ServerContext* context,
                                                 grpc::ServerReaderWriter<StreamingRecognizeResponse,
                                                 StreamingRecognizeRequest>* stream) {
    // Not implemented yet
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Method not implemented");
} 