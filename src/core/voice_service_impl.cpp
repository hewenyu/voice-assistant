#include "core/voice_service_impl.h"
#include "core/model_factory.h"

// C++ Standard Library
#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstring>
#include <uuid/uuid.h>
#include <sstream>

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
              << "  max_speech_duration: " << model_config_.vad.max_speech_duration << std::endl
              << "  window_size: " << model_config_.vad.window_size << std::endl
              << "  sample_rate: " << model_config_.vad.sample_rate << std::endl
              << "  num_threads: " << model_config_.vad.num_threads << std::endl
              << "  debug: " << model_config_.vad.debug << std::endl;

    // Zero initialization
    SherpaOnnxVadModelConfig config = {};
    vad_config_ = config;
    
    // Configure VAD with parameters from config
    vad_config_.silero_vad.model = model_config_.vad.model_path.c_str();
    vad_config_.silero_vad.threshold = model_config_.vad.threshold;
    vad_config_.silero_vad.min_silence_duration = model_config_.vad.min_silence_duration;
    vad_config_.silero_vad.min_speech_duration = model_config_.vad.min_speech_duration;
    vad_config_.silero_vad.max_speech_duration = model_config_.vad.max_speech_duration;
    vad_config_.silero_vad.window_size = model_config_.vad.window_size;
    vad_config_.sample_rate = model_config_.vad.sample_rate;
    vad_config_.num_threads = model_config_.vad.num_threads;
    vad_config_.debug = model_config_.vad.debug ? 1 : 0;

    // Create VAD with default buffer size (30 seconds should be enough for most cases)
    vad_ = SherpaOnnxCreateVoiceActivityDetector(&vad_config_, 30);
    if (!vad_) {
        std::cerr << "Failed to create VAD" << std::endl;
        return false;
    }
    return true;
}

bool VoiceServiceImpl::InitializeRecognizer() {
    std::cout << "Initializing recognizer with model: ";
    if (model_config_.type == "sense_voice") {
        std::cout << model_config_.sense_voice.model_path << std::endl;
    } else if (model_config_.type == "whisper") {
        std::cout << model_config_.whisper.encoder_path << " and " 
                  << model_config_.whisper.decoder_path << std::endl;
    }

    try {
        // For language auto-detection, we need audio samples
        // We'll create a small stream with some initial audio data
        const SherpaOnnxOfflineStream* stream = nullptr;
        std::vector<float> samples;
        
        if ((model_config_.type == "whisper" && model_config_.whisper.language == "auto") ||
            (model_config_.type == "sense_voice" && model_config_.sense_voice.language == "auto")) {
            
            // Create a temporary recognizer for getting audio samples
            auto temp_recognizer = ModelFactory::CreateModel(model_config_);
            if (!temp_recognizer) {
                std::cerr << "Failed to create temporary recognizer for language detection" << std::endl;
                return false;
            }
            
            // Create a stream
            stream = SherpaOnnxCreateOfflineStream(temp_recognizer);
            if (!stream) {
                std::cerr << "Failed to create stream for language detection" << std::endl;
                SherpaOnnxDestroyOfflineRecognizer(temp_recognizer);
                return false;
            }
            
            // Get some audio samples (you might want to adjust the size)
            const int sample_rate = 16000;  // Assuming 16kHz sample rate
            const int duration_ms = 5000;   // 5 seconds of audio
            samples.resize(sample_rate * duration_ms / 1000);
            
            // Now create the actual recognizer with language detection
            recognizer_ = ModelFactory::CreateModel(model_config_, samples.data(), samples.size());
            
            // Cleanup
            SherpaOnnxDestroyOfflineStream(stream);
            SherpaOnnxDestroyOfflineRecognizer(temp_recognizer);
        } else {
            // No language detection needed
            recognizer_ = ModelFactory::CreateModel(model_config_);
        }
        
        if (!recognizer_) {
            std::cerr << "Failed to create recognizer" << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create recognizer: " << e.what() << std::endl;
        return false;
    }
}

void VoiceServiceImpl::ConvertResults(const char* text, float confidence, SpeechRecognitionResult* result) {
    auto* alternative = result->add_alternatives();
    alternative->set_transcript(text);
    alternative->set_confidence(confidence);

    // 如果有词级别时间戳信息，添加到结果中
    if (text && *text) {
        std::string transcript(text);
        std::istringstream iss(transcript);
        std::string word;
        float start_time = 0.0f;
        const float word_duration = 0.3f;  // 假设每个词的持续时间为0.3秒

        while (iss >> word) {
            auto* word_info = alternative->add_words();
            word_info->set_word(word);
            
            // 设置开始时间
            auto* start = word_info->mutable_start_time();
            start->set_seconds(static_cast<int64_t>(start_time));
            start->set_nanos(static_cast<int32_t>((start_time - static_cast<int64_t>(start_time)) * 1e9));
            
            // 设置结束时间
            float end_time = start_time + word_duration;
            auto* end = word_info->mutable_end_time();
            end->set_seconds(static_cast<int64_t>(end_time));
            end->set_nanos(static_cast<int32_t>((end_time - static_cast<int64_t>(end_time)) * 1e9));
            
            start_time = end_time;
        }
    }
}

std::vector<SpeechRecognitionResult> VoiceServiceImpl::ProcessAudio(
    const std::string& audio_data, 
    const RecognitionConfig& config) {
    
    std::vector<SpeechRecognitionResult> results;
    if (!recognizer_ || !vad_) {
        std::cerr << "Recognizer or VAD not initialized" << std::endl;
        return results;
    }

    std::cout << "Processing audio data size: " << audio_data.size() << " bytes" << std::endl;

    // Convert audio data to float samples
    const int16_t* samples = reinterpret_cast<const int16_t*>(audio_data.data());
    int num_samples = audio_data.size() / sizeof(int16_t);
    std::vector<float> float_samples(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float_samples[i] = samples[i] / 32768.0f;
    }

    // Lock for model inference
    std::lock_guard<std::mutex> lock(recognition_mutex_);

    // Create a single stream for the entire audio
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!stream) {
        std::cerr << "Failed to create stream" << std::endl;
        return results;
    }

    // Process the entire audio
    SherpaOnnxAcceptWaveformOffline(stream, model_config_.vad.sample_rate, 
                                   float_samples.data(), float_samples.size());
    SherpaOnnxDecodeOfflineStream(recognizer_, stream);
    const SherpaOnnxOfflineRecognizerResult* result = 
        SherpaOnnxGetOfflineStreamResult(stream);

    if (result && result->text) {
        SpeechRecognitionResult recognition_result;
        auto* alternative = recognition_result.add_alternatives();
        alternative->set_transcript(result->text);
        alternative->set_confidence(1.0);

        // Count words and calculate timing
        std::istringstream word_counter(result->text);
        std::string word;
        int word_count = 0;
        while (word_counter >> word) {
            word_count++;
        }

        // Add word-level timing
        float total_duration = float_samples.size() / static_cast<float>(model_config_.vad.sample_rate);
        float word_duration = word_count > 0 ? total_duration / word_count : total_duration;
        float word_start = 0.0f;

        std::istringstream iss(result->text);
        while (iss >> word) {
            auto* word_info = alternative->add_words();
            word_info->set_word(word);
            
            // Set start time
            auto* start = word_info->mutable_start_time();
            start->set_seconds(static_cast<int64_t>(word_start));
            start->set_nanos(static_cast<int32_t>((word_start - static_cast<int64_t>(word_start)) * 1e9));
            
            // Set end time
            float word_end = word_start + word_duration;
            auto* end = word_info->mutable_end_time();
            end->set_seconds(static_cast<int64_t>(word_end));
            end->set_nanos(static_cast<int32_t>((word_end - static_cast<int64_t>(word_end)) * 1e9));
            
            word_start = word_end;
        }

        results.push_back(recognition_result);
    }

    SherpaOnnxDestroyOfflineRecognizerResult(result);
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

    // Convert audio data to float samples
    const int16_t* samples = reinterpret_cast<const int16_t*>(audio_data.data());
    size_t num_samples = audio_data.size() / sizeof(int16_t);
    if (num_samples == 0) return;

    std::vector<float> float_samples(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        float_samples[i] = samples[i] / 32768.0f;
    }

    // Process audio with VAD
    SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad_, float_samples.data(), float_samples.size());

    // Check if we have a complete speech segment
    bool has_complete_segment = false;
    if (!context.has_speech && SherpaOnnxVoiceActivityDetectorDetected(vad_)) {
        // Speech start detected
        std::lock_guard<std::mutex> lock(recognition_mutex_);
        if (context.stream) {
            SherpaOnnxDestroyOfflineStream(context.stream);
        }
        context.stream = SherpaOnnxCreateOfflineStream(recognizer_);
        context.has_speech = true;
        context.stability = 0.0;
        std::cout << "Speech start detected" << std::endl;
    } else if (context.has_speech && !SherpaOnnxVoiceActivityDetectorDetected(vad_)) {
        // Speech end detected
        has_complete_segment = true;
        std::cout << "Speech end detected" << std::endl;
    }

    // If we have speech, accumulate the audio
    if (context.has_speech && context.stream) {
        std::lock_guard<std::mutex> lock(recognition_mutex_);
        SherpaOnnxAcceptWaveformOffline(
            context.stream,
            context.config.config().sample_rate_hertz(),
            float_samples.data(),
            float_samples.size()
        );

        // For interim results during speech
        if (context.config.interim_results()) {
            SherpaOnnxDecodeOfflineStream(recognizer_, context.stream);
            const SherpaOnnxOfflineRecognizerResult* result = 
                SherpaOnnxGetOfflineStreamResult(context.stream);

            if (result && result->text) {
                std::cout << "Got new recognition result: " << result->text << std::endl;
                auto* streaming_result = response->add_results();
                streaming_result->set_is_final(false);
                auto* alternative = streaming_result->add_alternatives();
                alternative->set_transcript(result->text);
                alternative->set_confidence(0.0);
                context.stability += 0.1;
                streaming_result->set_stability(std::min(context.stability, 0.9f));
            }
            SherpaOnnxDestroyOfflineRecognizerResult(result);
        }
    }

    // Process complete segment
    if (has_complete_segment) {
        ProcessStreamingResult(context, response);
        context.has_speech = false;
        context.stability = 0.0;
    }
}

bool VoiceServiceImpl::ProcessStreamingResult(
    StreamContext& context,
    StreamingRecognizeResponse* response) {
    
    if (!context.stream) return false;

    std::lock_guard<std::mutex> lock(recognition_mutex_);
    SherpaOnnxDecodeOfflineStream(recognizer_, context.stream);
    const SherpaOnnxOfflineRecognizerResult* result = 
        SherpaOnnxGetOfflineStreamResult(context.stream);

    if (result && result->text) {
        std::cout << "Recognition result: " << result->text << std::endl;
        auto* streaming_result = response->add_results();
        streaming_result->set_is_final(true);
        auto* alternative = streaming_result->add_alternatives();
        alternative->set_transcript(result->text);
        alternative->set_confidence(1.0);
        streaming_result->set_stability(1.0);

        // Add word-level timing if enabled
        if (context.config.config().enable_word_time_offsets()) {
            // Count words
            std::istringstream word_counter(result->text);
            std::string word;
            int word_count = 0;
            while (word_counter >> word) {
                word_count++;
            }

            float total_duration = context.has_speech ? 
                (context.config.config().sample_rate_hertz() / 1000.0f) : 1.0f;
            float word_duration = word_count > 0 ? total_duration / word_count : total_duration;
            float word_start = 0.0f;

            // Add word timing
            std::istringstream iss(result->text);
            while (iss >> word) {
                auto* word_info = alternative->add_words();
                word_info->set_word(word);
                
                // Set start time
                auto* start = word_info->mutable_start_time();
                start->set_seconds(static_cast<int64_t>(word_start));
                start->set_nanos(static_cast<int32_t>((word_start - static_cast<int64_t>(word_start)) * 1e9));
                
                // Set end time
                float word_end = word_start + word_duration;
                auto* end = word_info->mutable_end_time();
                end->set_seconds(static_cast<int64_t>(word_end));
                end->set_nanos(static_cast<int32_t>((word_end - static_cast<int64_t>(word_end)) * 1e9));
                
                word_start = word_end;
            }
        }
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