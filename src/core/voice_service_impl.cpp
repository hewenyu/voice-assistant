#include "core/voice_service_impl.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <mutex>

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

    std::cout << "Processing audio data size: " << audio_data.size() << " bytes" << std::endl;

    // Create stream for this request
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!stream) {
        return "Failed to create stream";
    }

    // Convert audio data from int16 to float32
    std::vector<float> samples;
    samples.resize(audio_data.size() / 2);  // int16 is 2 bytes
    const int16_t* int16_data = reinterpret_cast<const int16_t*>(audio_data.data());
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = int16_data[i] / 32768.0f;  // Convert to float in range [-1, 1]
    }

    std::cout << "Converted " << samples.size() << " samples" << std::endl;

    // Accept audio data
    SherpaOnnxAcceptWaveformOffline(
        stream,
        16000, // sample rate
        samples.data(),
        samples.size()
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

struct StreamContext {
    const SherpaOnnxOfflineStream* stream = nullptr;
    std::vector<float> audio_buffer;
    std::string current_text;
    bool has_final_result = false;
};

void VoiceServiceImpl::ProcessStreamingAudio(
    StreamContext& context,
    const std::string& audio_data,
    StreamingRecognizeResponse* response
) {
    // 将音频数据转换为float格式
    const int16_t* int16_data = reinterpret_cast<const int16_t*>(audio_data.data());
    size_t num_samples = audio_data.size() / 2;  // int16 is 2 bytes
    
    // 添加到缓冲区
    size_t old_size = context.audio_buffer.size();
    context.audio_buffer.resize(old_size + num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        context.audio_buffer[old_size + i] = int16_data[i] / 32768.0f;
    }

    // 如果没有创建流，创建一个新的
    if (!context.stream) {
        context.stream = SherpaOnnxCreateOfflineStream(recognizer_);
    }

    // 处理音频数据
    SherpaOnnxAcceptWaveformOffline(
        context.stream,
        16000,  // sample rate
        context.audio_buffer.data(),
        context.audio_buffer.size()
    );

    // 进行识别
    SherpaOnnxDecodeOfflineStream(recognizer_, context.stream);
}

bool VoiceServiceImpl::ProcessStreamingResult(
    StreamContext& context,
    StreamingRecognizeResponse* response
) {
    // 获取识别结果
    const SherpaOnnxOfflineRecognizerResult* result = 
        SherpaOnnxGetOfflineStreamResult(context.stream);
    
    if (!result) {
        return false;
    }

    std::string new_text = result->text;
    
    // 如果有新的文本
    if (new_text != context.current_text) {
        context.current_text = new_text;
        response->set_is_final(false);
        response->set_text(new_text);
        return true;
    }

    return false;
}

grpc::Status VoiceServiceImpl::StreamingRecognize(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<StreamingRecognizeResponse,
    StreamingRecognizeRequest>* stream
) {
    StreamContext stream_context;
    StreamingRecognizeRequest request;
    StreamingRecognizeResponse response;

    // 读取客户端的流式请求
    while (stream->Read(&request)) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 处理音频数据
        if (request.has_audio_data()) {
            ProcessStreamingAudio(stream_context, request.audio_data(), &response);
            
            // 检查是否有新的识别结果
            if (ProcessStreamingResult(stream_context, &response)) {
                stream->Write(response);
            }
        }
    }

    // 处理最后的结果
    if (!stream_context.has_final_result) {
        response.set_is_final(true);
        response.set_text(stream_context.current_text);
        stream->Write(response);
    }

    return grpc::Status::OK;
} 