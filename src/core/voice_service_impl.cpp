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
    std::cout << "Initializing recognizer with model: ";
    if (model_config_.type == "sense_voice") {
        std::cout << model_config_.sense_voice.model_path << std::endl;
    } else if (model_config_.type == "whisper") {
        std::cout << model_config_.whisper.encoder_path << " and " 
                  << model_config_.whisper.decoder_path << std::endl;
    }

    try {
        recognizer_ = ModelFactory::CreateModel(model_config_);
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
    if (!recognizer_) {
        return results;
    }

    std::cout << "Processing audio data size: " << audio_data.size() << " bytes" << std::endl;

    // Convert audio data to float
    std::vector<float> samples(audio_data.size() / 2);
    const int16_t* audio_samples = reinterpret_cast<const int16_t*>(audio_data.data());
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<float>(audio_samples[i]) / 32768.0f;
    }

    // Create a stream to process the entire audio
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!stream) {
        return results;
    }

    // Process the entire audio
    SherpaOnnxAcceptWaveformOffline(stream, model_config_.vad.sample_rate, samples.data(), samples.size());
    SherpaOnnxDecodeOfflineStream(recognizer_, stream);

    const SherpaOnnxOfflineRecognizerResult* result = SherpaOnnxGetOfflineStreamResult(stream);
    if (result && result->text) {
        SpeechRecognitionResult recognition_result;
        auto* alternative = recognition_result.add_alternatives();
        
        // Process text while preserving UTF-8 encoding and adding spaces
        std::string text = result->text;
        std::string clean_text;
        const char* ptr = text.c_str();
        bool need_space = false;

        while (*ptr) {
            if (static_cast<unsigned char>(*ptr) < 0x80) {
                // ASCII character
                if (*ptr == '.' || *ptr == ',' || *ptr == ' ') {
                    // Skip punctuation but add space if needed
                    if (need_space && !clean_text.empty() && clean_text.back() != ' ') {
                        clean_text += ' ';
                    }
                    need_space = false;
                } else {
                    if (need_space && !clean_text.empty() && clean_text.back() != ' ') {
                        clean_text += ' ';
                    }
                    clean_text += *ptr;
                    need_space = true;
                }
                ptr++;
            } else {
                // UTF-8 character
                int len = 1;
                if ((static_cast<unsigned char>(*ptr) & 0xE0) == 0xC0) len = 2;
                else if ((static_cast<unsigned char>(*ptr) & 0xF0) == 0xE0) len = 3;
                else if ((static_cast<unsigned char>(*ptr) & 0xF8) == 0xF0) len = 4;
                
                // Skip Chinese/Japanese punctuation marks
                bool is_punctuation = false;
                if (len == 3) {
                    const char* punct = ptr;
                    if ((static_cast<unsigned char>(punct[0]) == 0xE3 && 
                         static_cast<unsigned char>(punct[1]) == 0x80 && 
                         static_cast<unsigned char>(punct[2]) == 0x82) || // 。
                        (static_cast<unsigned char>(punct[0]) == 0xEF && 
                         static_cast<unsigned char>(punct[1]) == 0xBC && 
                         static_cast<unsigned char>(punct[2]) == 0x8C) || // ，
                        (static_cast<unsigned char>(punct[0]) == 0xE3 && 
                         static_cast<unsigned char>(punct[1]) == 0x80 && 
                         static_cast<unsigned char>(punct[2]) == 0x81)) { // 、
                        is_punctuation = true;
                    }
                }
                
                if (!is_punctuation) {
                    clean_text.append(ptr, len);
                }
                ptr += len;
            }
        }
        
        alternative->set_transcript(clean_text);
        alternative->set_confidence(1.0f);

        // Add word-level timestamps
        float total_duration = static_cast<float>(samples.size()) / model_config_.vad.sample_rate;
        float current_time = 0.0f;
        
        // Split text into words
        std::vector<std::string> words;
        std::string current_word;
        const char* word_ptr = clean_text.c_str();
        
        while (*word_ptr) {
            if (static_cast<unsigned char>(*word_ptr) < 0x80) {
                // ASCII character
                if (*word_ptr == ' ') {
                    if (!current_word.empty()) {
                        words.push_back(current_word);
                        current_word.clear();
                    }
                } else {
                    current_word += *word_ptr;
                }
                word_ptr++;
            } else {
                // UTF-8 character
                int len = 1;
                if ((static_cast<unsigned char>(*word_ptr) & 0xE0) == 0xC0) len = 2;
                else if ((static_cast<unsigned char>(*word_ptr) & 0xF0) == 0xE0) len = 3;
                else if ((static_cast<unsigned char>(*word_ptr) & 0xF8) == 0xF0) len = 4;
                
                if (!current_word.empty()) {
                    words.push_back(current_word);
                    current_word.clear();
                }
                
                current_word.append(word_ptr, len);
                words.push_back(current_word);
                current_word.clear();
                
                word_ptr += len;
            }
        }
        
        if (!current_word.empty()) {
            words.push_back(current_word);
        }
        
        // Add timestamps for each word
        if (!words.empty()) {
            float word_duration = total_duration / words.size();
            
            for (const auto& word : words) {
                auto* word_info = alternative->add_words();
                word_info->set_word(word);
                
                auto* start = word_info->mutable_start_time();
                start->set_seconds(static_cast<int64_t>(current_time));
                start->set_nanos(static_cast<int32_t>((current_time - 
                    static_cast<int64_t>(current_time)) * 1e9));
                
                float end_time = current_time + word_duration;
                auto* end = word_info->mutable_end_time();
                end->set_seconds(static_cast<int64_t>(end_time));
                end->set_nanos(static_cast<int32_t>((end_time - 
                    static_cast<int64_t>(end_time)) * 1e9));
                
                current_time = end_time;
            }
        }

        results.push_back(recognition_result);
    }

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