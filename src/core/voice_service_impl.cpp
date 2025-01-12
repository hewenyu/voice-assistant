#include "core/voice_service_impl.h"

// C++ Standard Library
#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstring>
#include <uuid/uuid.h>

namespace voice {

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
    std::cout << "Initializing VAD with model: " << model_config_.vad.model_path << std::endl;
    std::cout << "VAD parameters:" << std::endl
              << "  threshold: " << model_config_.vad.threshold << std::endl
              << "  min_silence_duration: " << model_config_.vad.min_silence_duration << std::endl
              << "  min_speech_duration: " << model_config_.vad.min_speech_duration << std::endl
              << "  window_size: " << model_config_.vad.window_size << std::endl;

    // Zero initialization
    SherpaOnnxVadModelConfig config = {};
    vad_config_ = config;
    
    // Configure VAD with more sensitive parameters
    vad_config_.silero_vad.model = model_config_.vad.model_path.c_str();
    vad_config_.silero_vad.threshold = model_config_.vad.threshold;
    vad_config_.silero_vad.min_silence_duration = model_config_.vad.min_silence_duration;
    vad_config_.silero_vad.min_speech_duration = model_config_.vad.min_speech_duration;
    vad_config_.silero_vad.max_speech_duration = model_config_.vad.max_speech_duration;
    vad_config_.silero_vad.window_size = model_config_.vad.window_size;
    vad_config_.sample_rate = model_config_.vad.sample_rate;
    vad_config_.num_threads = model_config_.num_threads;
    vad_config_.debug = model_config_.debug ? 1 : 0;

    // Create VAD
    vad_ = SherpaOnnxCreateVoiceActivityDetector(&vad_config_, 120);
    if (!vad_) {
        std::cerr << "Failed to create VAD" << std::endl;
        return false;
    }
    return true;
}

bool VoiceServiceImpl::InitializeRecognizer() {
    std::cout << "Initializing recognizer with model: " << model_config_.sense_voice.model_path << std::endl;
    
    // Zero initialization
    SherpaOnnxOfflineRecognizerConfig cfg = {};
    config_ = cfg;

    // Set model paths with zero initialization
    SherpaOnnxOfflineSenseVoiceModelConfig sense_voice_config = {};
    sense_voice_config.model = model_config_.sense_voice.model_path.c_str();
    sense_voice_config.language = model_config_.sense_voice.language.c_str();
    sense_voice_config.use_itn = model_config_.sense_voice.use_itn ? 1 : 0;

    // Offline model config with zero initialization
    SherpaOnnxOfflineModelConfig model_config = {};
    model_config.debug = model_config_.debug ? 1 : 0;
    model_config.num_threads = model_config_.num_threads;
    model_config.provider = model_config_.provider.c_str();
    model_config.tokens = model_config_.sense_voice.tokens_path.c_str();
    model_config.sense_voice = sense_voice_config;

    config_.model_config = model_config;
    config_.decoding_method = model_config_.sense_voice.decoding_method.c_str();

    // Create recognizer
    recognizer_ = SherpaOnnxCreateOfflineRecognizer(&config_);
    if (!recognizer_) {
        std::cerr << "Failed to create recognizer" << std::endl;
        return false;
    }
    return true;
}

void VoiceServiceImpl::ConvertResults(const char* text, float confidence, SpeechRecognitionResult* result) {
    auto* alternative = result->add_alternatives();
    alternative->set_transcript(text);
    alternative->set_confidence(confidence);
}

std::vector<SpeechRecognitionResult> VoiceServiceImpl::ProcessAudio(
    const std::string& audio_data, 
    const RecognitionConfig& config) {
    
    std::vector<SpeechRecognitionResult> results;
    if (!recognizer_) {
        return results;
    }

    std::cout << "Processing audio data size: " << audio_data.size() << " bytes" << std::endl;

    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!stream) {
        return results;
    }

    // Convert audio data from int16 to float32
    std::vector<float> samples;
    samples.resize(audio_data.size() / 2);
    const int16_t* int16_data = reinterpret_cast<const int16_t*>(audio_data.data());
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = int16_data[i] / 32768.0f;
    }

    SherpaOnnxAcceptWaveformOffline(
        stream,
        config.sample_rate_hertz(),
        samples.data(),
        samples.size()
    );

    SherpaOnnxDecodeOfflineStream(recognizer_, stream);

    const SherpaOnnxOfflineRecognizerResult* sherpa_result = 
        SherpaOnnxGetOfflineStreamResult(stream);

    if (sherpa_result && sherpa_result->text) {
        SpeechRecognitionResult result;
        ConvertResults(sherpa_result->text, 0.9, &result);
        results.push_back(result);
    }

    SherpaOnnxDestroyOfflineRecognizerResult(sherpa_result);
    SherpaOnnxDestroyOfflineStream(stream);

    return results;
}

grpc::Status VoiceServiceImpl::SyncRecognize(
    grpc::ServerContext* context,
    const SyncRecognizeRequest* request,
    SyncRecognizeResponse* response) {
    
    std::string audio_data;
    if (request->has_audio_content()) {
        audio_data = request->audio_content();
    } else if (request->has_uri()) {
        // TODO: Implement GCS file reading
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, 
                          "GCS file reading not implemented");
    }

    auto results = ProcessAudio(audio_data, request->config());
    for (const auto& result : results) {
        *response->add_results() = result;
    }
    
    return grpc::Status::OK;
}

grpc::Status VoiceServiceImpl::AsyncRecognize(
    grpc::ServerContext* context,
    const AsyncRecognizeRequest* request,
    AsyncRecognizeResponse* response) {
    
    // Generate operation ID
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    std::string operation_id(uuid_str);

    // Store operation
    {
        std::lock_guard<std::mutex> lock(async_mutex_);
        AsyncOperation op;
        op.request_id = request->request_id();
        op.status = GetAsyncRecognizeStatusResponse::RUNNING;
        async_operations_[operation_id] = op;
    }

    // Start async processing
    std::thread([this, request, operation_id]() {
        std::string audio_data;
        if (request->has_audio_content()) {
            audio_data = request->audio_content();
        } else if (request->has_uri()) {
            // TODO: Implement GCS file reading
            return;
        }

        auto results = ProcessAudio(audio_data, request->config());

        std::lock_guard<std::mutex> lock(async_mutex_);
        auto& op = async_operations_[operation_id];
        op.status = GetAsyncRecognizeStatusResponse::SUCCEEDED;
        op.results = results;
    }).detach();

    response->set_request_id(request->request_id());
    response->set_operation_id(operation_id);
    
    return grpc::Status::OK;
}

grpc::Status VoiceServiceImpl::GetAsyncRecognizeStatus(
    grpc::ServerContext* context,
    const GetAsyncRecognizeStatusRequest* request,
    GetAsyncRecognizeStatusResponse* response) {
    
    std::lock_guard<std::mutex> lock(async_mutex_);
    auto it = async_operations_.find(request->operation_id());
    if (it == async_operations_.end()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Operation not found");
    }

    const auto& op = it->second;
    response->set_status(op.status);
    if (op.status == GetAsyncRecognizeStatusResponse::SUCCEEDED) {
        for (const auto& result : op.results) {
            *response->add_results() = result;
        }
    } else if (op.status == GetAsyncRecognizeStatusResponse::FAILED) {
        response->set_error(op.error);
    }

    return grpc::Status::OK;
}

void VoiceServiceImpl::ProcessStreamingAudio(
    StreamContext& context,
    const std::string& audio_data,
    StreamingRecognizeResponse* response) {
    
    if (!context.is_initialized || !vad_) {
        return;
    }

    const int16_t* int16_data = reinterpret_cast<const int16_t*>(audio_data.data());
    size_t num_samples = audio_data.size() / 2;
    if (num_samples == 0) return;

    std::vector<float> samples(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        samples[i] = int16_data[i] / 32768.0f;
    }

    SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad_, samples.data(), samples.size());
    bool is_speech = SherpaOnnxVoiceActivityDetectorDetected(vad_);

    if (is_speech && !context.was_speech) {
        if (context.stream) {
            SherpaOnnxDestroyOfflineStream(context.stream);
        }
        context.stream = SherpaOnnxCreateOfflineStream(recognizer_);
        context.alternatives.clear();
        context.has_speech = true;
        context.stability = 0.0;
    }

    if (is_speech && context.stream) {
        SherpaOnnxAcceptWaveformOffline(
            context.stream,
            context.config.config().sample_rate_hertz(),
            samples.data(),
            samples.size()
        );

        if (context.config.interim_results()) {
            SherpaOnnxDecodeOfflineStream(recognizer_, context.stream);
            const SherpaOnnxOfflineRecognizerResult* result = 
                SherpaOnnxGetOfflineStreamResult(context.stream);

            if (result && result->text) {
                auto* streaming_result = response->add_results();
                streaming_result->set_is_final(false);
                auto* alternative = streaming_result->add_alternatives();
                alternative->set_transcript(result->text);
                alternative->set_confidence(0.0);  // Interim results don't have confidence
                context.stability += 0.1;  // Increase stability as we process more audio
                streaming_result->set_stability(std::min(context.stability, 0.9f));
            }
            SherpaOnnxDestroyOfflineRecognizerResult(result);
        }
    }

    if (!is_speech && context.was_speech) {
        ProcessStreamingResult(context, response);
        context.has_speech = false;
        context.stability = 0.0;
    }

    context.was_speech = is_speech;
}

bool VoiceServiceImpl::ProcessStreamingResult(
    StreamContext& context,
    StreamingRecognizeResponse* response) {
    
    if (!context.stream) return false;

    SherpaOnnxDecodeOfflineStream(recognizer_, context.stream);
    const SherpaOnnxOfflineRecognizerResult* result = 
        SherpaOnnxGetOfflineStreamResult(context.stream);

    if (result && result->text) {
        auto* streaming_result = response->add_results();
        streaming_result->set_is_final(true);
        auto* alternative = streaming_result->add_alternatives();
        alternative->set_transcript(result->text);
        alternative->set_confidence(0.9);
        streaming_result->set_stability(1.0);
    }

    SherpaOnnxDestroyOfflineRecognizerResult(result);
    SherpaOnnxDestroyOfflineStream(context.stream);
    context.stream = nullptr;

    return true;
}

grpc::Status VoiceServiceImpl::StreamingRecognize(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<StreamingRecognizeResponse,
    StreamingRecognizeRequest>* stream) {
    
    StreamContext stream_context;
    StreamingRecognizeRequest request;

    while (stream->Read(&request)) {
        StreamingRecognizeResponse response;

        if (request.has_streaming_config()) {
            stream_context.is_initialized = true;
            stream_context.config = request.streaming_config();
            continue;
        }

        if (!stream_context.is_initialized) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, 
                              "First message must contain streaming config");
        }

        ProcessStreamingAudio(stream_context, request.audio_content(), &response);
        
        if (response.results_size() > 0) {
            stream->Write(response);
        }

        if (stream_context.config.single_utterance() && 
            response.results_size() > 0 && 
            response.results(0).is_final()) {
            break;
        }
    }

    return grpc::Status::OK;
}

} // namespace voice 