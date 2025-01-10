#include "voice_service_impl.h"
#include <grpcpp/grpcpp.h>
#include <vector>
#include <cstring>

namespace voice {

VoiceServiceImpl::VoiceServiceImpl()
    : recognizer_(nullptr)
    , stream_(nullptr)
    , initialized_(false) {
    // 设置模型目录，这里应该从配置文件读取
    model_dir_ = "models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue";
    InitializeRecognizer();
}

VoiceServiceImpl::~VoiceServiceImpl() {
    if (stream_) {
        sherpa_onnx_online_recognizer_delete_stream(recognizer_, stream_);
    }
    if (recognizer_) {
        sherpa_onnx_online_recognizer_delete(recognizer_);
    }
}

bool VoiceServiceImpl::InitializeRecognizer() {
    if (initialized_) return true;

    // 配置识别器
    struct SherpaOnnxOnlineRecognizerConfig config;
    
    // 设置模型路径
    config.model_config.encoder = (model_dir_ + "/encoder.onnx").c_str();
    config.model_config.decoder = (model_dir_ + "/decoder.onnx").c_str();
    config.model_config.tokens = (model_dir_ + "/tokens.txt").c_str();
    
    // 设置特征提取参数
    config.feat_config.sample_rate = 16000;
    config.feat_config.feature_dim = 80;
    
    // 设置解码参数
    config.decoder_config.decoding_method = "greedy_search";
    config.decoder_config.num_active_paths = 4;
    
    config.enable_endpoint = 1;
    config.max_active_paths = 4;
    
    // 创建识别器
    recognizer_ = sherpa_onnx_online_recognizer_new(&config);
    if (!recognizer_) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

RecognitionResult VoiceServiceImpl::ProcessAudio(
    const std::string& audio_content,
    const RecognitionConfig& config) {
    
    RecognitionResult result;
    
    // 创建新的流
    stream_ = sherpa_onnx_online_recognizer_create_stream(recognizer_);
    if (!stream_) {
        return result;
    }
    
    // 将 base64 编码的音频数据转换为 float 数组
    std::vector<float> samples;
    // TODO: 实现 base64 解码和音频格式转换
    
    // 处理音频数据
    sherpa_onnx_online_stream_accept_waveform(
        stream_,
        config.sample_rate(),
        samples.data(),
        samples.size()
    );
    
    // 获取识别结果
    struct SherpaOnnxOnlineRecognizerResult recog_result = 
        sherpa_onnx_online_recognizer_decode(recognizer_);
    
    // 填充结果
    result.set_transcript(recog_result.text);
    result.set_confidence(0.98); // TODO: 从 sherpa-onnx 获取实际的置信度
    
    // 如果启用了词时间戳
    if (config.enable_word_timestamps()) {
        for (int i = 0; i < recog_result.num_words; ++i) {
            auto* word_info = result.add_words();
            word_info->set_word(recog_result.words[i].word);
            word_info->set_start_time(recog_result.words[i].start_time);
            word_info->set_end_time(recog_result.words[i].end_time);
            word_info->set_confidence(recog_result.words[i].confidence);
        }
    }
    
    // 清理流
    sherpa_onnx_online_recognizer_delete_stream(recognizer_, stream_);
    stream_ = nullptr;
    
    return result;
}

grpc::Status VoiceServiceImpl::SyncRecognize(
    grpc::ServerContext* context,
    const SyncRecognizeRequest* request,
    SyncRecognizeResponse* response) {
    
    if (!initialized_ && !InitializeRecognizer()) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                          "Failed to initialize recognizer");
    }
    
    auto result = ProcessAudio(request->audio_content(), request->config());
    *response->add_results() = std::move(result);
    
    return grpc::Status::OK;
}

grpc::Status VoiceServiceImpl::AsyncRecognize(
    grpc::ServerContext* context,
    const AsyncRecognizeRequest* request,
    AsyncRecognizeResponse* response) {
    
    // TODO: 实现异步识别
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented yet");
}

grpc::Status VoiceServiceImpl::GetAsyncRecognizeStatus(
    grpc::ServerContext* context,
    const GetAsyncRecognizeStatusRequest* request,
    AsyncRecognizeResponse* response) {
    
    // TODO: 实现获取异步识别状态
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented yet");
}

grpc::Status VoiceServiceImpl::StreamingRecognize(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<StreamingRecognizeResponse,
                            StreamingRecognizeRequest>* stream) {
    
    if (!initialized_ && !InitializeRecognizer()) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                          "Failed to initialize recognizer");
    }
    
    StreamingRecognizeRequest request;
    StreamingRecognizeResponse response;
    
    // 创建新的流
    stream_ = sherpa_onnx_online_recognizer_create_stream(recognizer_);
    if (!stream_) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                          "Failed to create stream");
    }
    
    // 读取客户端的流式请求
    while (stream->Read(&request)) {
        if (request.has_config()) {
            // 处理配置
            continue;
        }
        
        // 处理音频数据
        std::vector<float> samples;
        // TODO: 实现 base64 解码和音频格式转换
        
        sherpa_onnx_online_stream_accept_waveform(
            stream_,
            16000, // 使用配置中的采样率
            samples.data(),
            samples.size()
        );
        
        // 获取识别结果
        struct SherpaOnnxOnlineRecognizerResult recog_result = 
            sherpa_onnx_online_recognizer_decode(recognizer_);
        
        // 填充响应
        auto* result = response.add_results();
        result->set_transcript(recog_result.text);
        result->set_confidence(0.98); // TODO: 获取实际的置信度
        
        // 发送响应
        response.set_is_final(false);
        stream->Write(response);
    }
    
    // 发送最终结果
    response.set_is_final(true);
    stream->Write(response);
    
    // 清理流
    sherpa_onnx_online_recognizer_delete_stream(recognizer_, stream_);
    stream_ = nullptr;
    
    return grpc::Status::OK;
}

} // namespace voice 