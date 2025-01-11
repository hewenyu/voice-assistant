#include "core/voice_service_impl.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdio>

VoiceServiceImpl::VoiceServiceImpl(const ModelConfig& config) 
    : recognizer_(nullptr)
    , model_config_(config)
    , vad_(nullptr) {
    std::cout << "Initializing VoiceServiceImpl..." << std::endl;
    
    if (!InitializeRecognizer()) {
        throw std::runtime_error("Failed to initialize recognizer");
    }
    std::cout << "Recognizer initialized successfully" << std::endl;

    if (!InitializeVAD()) {
        throw std::runtime_error("Failed to initialize VAD");
    }
    std::cout << "VAD initialized successfully" << std::endl;
}

VoiceServiceImpl::~VoiceServiceImpl() {
    std::cout << "Destroying VoiceServiceImpl..." << std::endl;
    if (recognizer_) {
        SherpaOnnxDestroyOfflineRecognizer(recognizer_);
        std::cout << "Recognizer destroyed" << std::endl;
    }
    if (vad_) {
        SherpaOnnxDestroyVoiceActivityDetector(vad_);
        std::cout << "VAD destroyed" << std::endl;
    }
}

bool VoiceServiceImpl::InitializeVAD() {
    std::cout << "Initializing VAD with model: " << model_config_.vad_model_path << std::endl;
    std::cout << "VAD parameters:" << std::endl
              << "  threshold: " << model_config_.vad_threshold << std::endl
              << "  min_silence_duration: " << model_config_.vad_min_silence_duration << std::endl
              << "  min_speech_duration: " << model_config_.vad_min_speech_duration << std::endl
              << "  window_size: " << model_config_.vad_window_size << std::endl;

    std::memset(&vad_config_, 0, sizeof(vad_config_));
    
    // Configure VAD with more sensitive parameters
    vad_config_.silero_vad.model = model_config_.vad_model_path.c_str();
    vad_config_.silero_vad.threshold = 0.5f;  // 提高阈值，减少误检
    vad_config_.silero_vad.min_silence_duration = 0.5f;  // 增加静音持续时间，更好地分割句子
    vad_config_.silero_vad.min_speech_duration = 0.25f;  // 增加最小语音持续时间，减少碎片
    vad_config_.silero_vad.max_speech_duration = 20.0f;  // 减少最大语音持续时间，避免过长
    vad_config_.silero_vad.window_size = 512;  // 使用较小的窗口大小，提高实时性
    vad_config_.sample_rate = model_config_.sample_rate;
    vad_config_.num_threads = model_config_.num_threads;
    vad_config_.debug = model_config_.debug ? 1 : 0;

    // 创建VAD，使用更大的初始缓冲区大小 (2M samples)
    vad_ = SherpaOnnxCreateVoiceActivityDetector(&vad_config_, 120);  // 增加缓冲区大小
    if (!vad_) {
        std::cerr << "Failed to create VAD" << std::endl;
        return false;
    }
    return true;
}

bool VoiceServiceImpl::InitializeRecognizer() {
    std::cout << "Initializing recognizer with model: " << model_config_.model_path << std::endl;
    
    // Configure sherpa-onnx
    std::memset(&config_, 0, sizeof(config_));

    // Set model paths
    SherpaOnnxOfflineSenseVoiceModelConfig sense_voice_config;
    memset(&sense_voice_config, 0, sizeof(sense_voice_config));
    sense_voice_config.model = model_config_.model_path.c_str();
    sense_voice_config.language = model_config_.language.c_str();
    sense_voice_config.use_itn = model_config_.use_itn ? 1 : 0;

    // Offline model config
    SherpaOnnxOfflineModelConfig model_config;
    memset(&model_config, 0, sizeof(model_config));
    model_config.debug = model_config_.debug ? 1 : 0;
    model_config.num_threads = model_config_.num_threads;
    model_config.provider = model_config_.provider.c_str();
    model_config.tokens = model_config_.tokens_path.c_str();
    model_config.sense_voice = sense_voice_config;

    config_.model_config = model_config;
    config_.decoding_method = model_config_.decoding_method.c_str();

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
        model_config_.sample_rate,
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
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Method not implemented");
}

grpc::Status VoiceServiceImpl::GetAsyncRecognizeStatus(grpc::ServerContext* context,
                                                      const GetAsyncRecognizeStatusRequest* request,
                                                      GetAsyncRecognizeStatusResponse* response) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Method not implemented");
}

void VoiceServiceImpl::ProcessStreamingAudio(
    StreamContext& context,
    const std::string& audio_data,
    StreamingRecognizeResponse* response
) {
    if (!vad_) {
        std::cerr << "VAD not initialized!" << std::endl;
        return;
    }

    // Convert audio data to int16 format
    const int16_t* int16_data = reinterpret_cast<const int16_t*>(audio_data.data());
    size_t num_samples = audio_data.size() / 2;  // int16 is 2 bytes
    if (num_samples == 0) {
        return;
    }

    // Convert to float format
    std::vector<float> samples(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        samples[i] = int16_data[i] / 32768.0f;
    }

    try {
        // Feed audio data to VAD
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad_, samples.data(), samples.size());

        // Create new stream if needed
        if (!context.stream) {
            context.stream = SherpaOnnxCreateOfflineStream(recognizer_);
            if (!context.stream) {
                std::cerr << "Failed to create stream!" << std::endl;
                return;
            }
        }

        // Add current audio data to stream
        SherpaOnnxAcceptWaveformOffline(
            context.stream,
            model_config_.sample_rate,
            samples.data(),
            samples.size()
        );

        // Process VAD segments
        bool has_speech = false;
        bool has_silence = false;
        
        while (!SherpaOnnxVoiceActivityDetectorEmpty(vad_)) {
            const SherpaOnnxSpeechSegment* segment = SherpaOnnxVoiceActivityDetectorFront(vad_);
            if (!segment) {
                break;
            }

            if (segment->n > 0) {
                has_speech = true;
                context.has_speech = true;
                context.continuous_silence_chunks = 0;
            } else {
                has_silence = true;
                if (context.has_speech) {
                    context.continuous_silence_chunks++;
                }
            }

            SherpaOnnxDestroySpeechSegment(segment);
            SherpaOnnxVoiceActivityDetectorPop(vad_);
        }

        // 每次都尝试解码，不管是否有新的语音
        SherpaOnnxDecodeOfflineStream(recognizer_, context.stream);
        
        // 获取当前结果
        const SherpaOnnxOfflineRecognizerResult* result = 
            SherpaOnnxGetOfflineStreamResult(context.stream);
        
        if (result) {
            std::string new_text = result->text;
            if (!new_text.empty()) {
                // 检查是否需要结束当前句子
                // 1. 有足够长的静音
                // 2. 文本内容发生了变化
                bool should_finalize = context.continuous_silence_chunks >= 2 && 
                                     new_text != context.current_text;

                // 只有当文本变化时才发送结果
                if (new_text != context.current_text) {
                    response->set_text(new_text);
                    response->set_is_final(should_finalize);
                    context.current_text = new_text;

                    // 如果是最终结果，重置状态
                    if (should_finalize) {
                        if (context.stream) {
                            SherpaOnnxDestroyOfflineStream(context.stream);
                            context.stream = nullptr;
                        }
                        context.current_text.clear();
                        context.has_speech = false;
                        context.continuous_silence_chunks = 0;
                    }
                }
            }
            SherpaOnnxDestroyOfflineRecognizerResult(result);
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception in ProcessStreamingAudio: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in ProcessStreamingAudio" << std::endl;
    }
}

grpc::Status VoiceServiceImpl::StreamingRecognize(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<StreamingRecognizeResponse,
    StreamingRecognizeRequest>* stream
) {
    StreamContext stream_context;
    StreamingRecognizeRequest request;
    StreamingRecognizeResponse response;

    while (stream->Read(&request)) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (request.has_audio_data()) {
            ProcessStreamingAudio(stream_context, request.audio_data(), &response);
            
            // 只有当有文本时才发送响应
            if (!response.text().empty()) {
                stream->Write(response);
            }
        }
    }

    return grpc::Status::OK;
} 