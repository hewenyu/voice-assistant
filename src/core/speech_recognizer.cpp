#include "core/speech_recognizer.h"
#include <stdexcept>
#include <sndfile.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace voice_assistant {

SpeechRecognizer::SpeechRecognizer() 
    : context_(nullptr), initialized_(false), state_(RecognitionState::IDLE) {}

SpeechRecognizer::~SpeechRecognizer() {
    if (context_) {
        if (context_->state) {
            sense_voice_free_state(context_->state);
            context_->state = nullptr;
        }
        // 释放context本身
        // TODO: 需要添加 sense_voice_free_context 函数
        context_ = nullptr;
    }
}

bool SpeechRecognizer::initialize(const std::string& model_path) {
    try {
        if (context_) {
            if (context_->state) {
                sense_voice_free_state(context_->state);
                context_->state = nullptr;
            }
            context_ = nullptr;
        }

        std::cout << "Initializing speech recognizer with model: " << model_path << std::endl;

        auto params = sense_voice_context_default_params();
        params.use_gpu = false;  // 禁用GPU加速，使用CPU模式
        
        // 初始化上下文
        context_ = sense_voice_init_with_params_no_state(
            model_path.c_str(),
            params
        );
        
        if (!context_) {
            std::cerr << "Failed to initialize sense_voice context" << std::endl;
            return false;
        }

        // 验证上下文状态
        if (!context_->model.ctx) {
            std::cerr << "Model context is null" << std::endl;
            return false;
        }

        // 验证模型是否正确加载
        if (!context_->model.buffer) {
            std::cerr << "Model buffer is null" << std::endl;
            return false;
        }

        std::cout << "Speech recognizer initialized successfully" << std::endl;
        initialized_ = true;
        state_ = RecognitionState::IDLE;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing recognizer: " << e.what() << std::endl;
        state_ = RecognitionState::ERROR;
        return false;
    }
}

// 内部处理方法
RecognitionResult SpeechRecognizer::process_recognition(
    const std::vector<float>& audio_data,
    const RecognitionConfig& config) {
    
    if (!context_) {
        throw std::runtime_error("Recognizer not initialized");
    }

    if (!context_->state) {
        throw std::runtime_error("Recognizer state not initialized");
    }

    try {
        // 设置识别参数
        sense_voice_full_params params = sense_voice_full_default_params(SENSE_VOICE_SAMPLING_GREEDY);
        params.language = config.language_code.c_str();
        
        // 将 float 转换为 double
        std::vector<double> samples;
        samples.reserve(audio_data.size());
        for (float sample : audio_data) {
            samples.push_back(static_cast<double>(sample));
        }

        // 清除之前的结果
        if (context_->state) {
            context_->state->result_all.clear();
        }

        // 执行语音识别
        int result = sense_voice_full_parallel(
            context_,
            params,
            samples,
            samples.size(),
            1  // num_threads
        );

        if (result != 0) {
            throw std::runtime_error("Failed to perform speech recognition");
        }

        // 获取识别结果
        RecognitionResult recognition_result;
        if (context_->state && !context_->state->result_all.empty()) {
            const auto& segment = context_->state->result_all[0];
            recognition_result.transcript = segment.text;
            recognition_result.confidence = 1.0;
            recognition_result.is_final = true;
            recognition_result.start_time = segment.t0;
            recognition_result.end_time = segment.t1;
            // TODO: 实现分词功能
        } else {
            recognition_result.transcript = "";
            recognition_result.confidence = 0.0;
            recognition_result.is_final = true;
            recognition_result.start_time = 0.0;
            recognition_result.end_time = 0.0;
        }

        return recognition_result;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Speech recognition failed: ") + e.what());
    }
}

// 同步识别实现
RecognitionResult SpeechRecognizer::recognize_sync(
    const std::string& audio_path,
    const RecognitionConfig& config) {
    
    if (!initialized_) {
        throw std::runtime_error("Recognizer not initialized");
    }

    state_ = RecognitionState::RECOGNIZING;
    auto audio_data = preprocess_audio(audio_path);
    auto result = process_recognition(audio_data, config);
    state_ = RecognitionState::FINISHED;
    return result;
}

RecognitionResult SpeechRecognizer::recognize_sync(
    const std::vector<float>& audio_data,
    const RecognitionConfig& config) {
    
    if (!initialized_) {
        throw std::runtime_error("Recognizer not initialized");
    }

    state_ = RecognitionState::RECOGNIZING;
    auto result = process_recognition(audio_data, config);
    state_ = RecognitionState::FINISHED;
    return result;
}

// 异步识别实现
std::future<RecognitionResult> SpeechRecognizer::recognize_async(
    const std::string& audio_path,
    const RecognitionConfig& config) {
    
    return std::async(std::launch::async, [this, audio_path, config]() {
        return recognize_sync(audio_path, config);
    });
}

std::future<RecognitionResult> SpeechRecognizer::recognize_async(
    const std::vector<float>& audio_data,
    const RecognitionConfig& config) {
    
    return std::async(std::launch::async, [this, audio_data, config]() {
        return recognize_sync(audio_data, config);
    });
}

// 流式识别实现
bool SpeechRecognizer::start_streaming(
    const RecognitionConfig& config,
    StreamingRecognitionCallback callback) {
    
    if (!initialized_ || is_streaming_) {
        return false;
    }

    is_streaming_ = true;
    streaming_callback_ = callback;
    audio_buffer_.clear();
    state_ = RecognitionState::RECOGNIZING;
    return true;
}

bool SpeechRecognizer::feed_audio(const std::vector<float>& audio_chunk) {
    if (!is_streaming_) {
        return false;
    }

    // 添加新的音频数据到缓冲区
    audio_buffer_.insert(audio_buffer_.end(), audio_chunk.begin(), audio_chunk.end());

    // 如果累积了足够的数据，进行识别
    if (audio_buffer_.size() >= 16000) {  // 假设采样率为16kHz，处理1秒的数据
        try {
            RecognitionConfig config;
            auto result = process_recognition(audio_buffer_, config);
            if (streaming_callback_) {
                streaming_callback_(result);
            }
            audio_buffer_.clear();
        } catch (const std::exception& e) {
            std::cerr << "Streaming recognition error: " << e.what() << std::endl;
            return false;
        }
    }

    return true;
}

bool SpeechRecognizer::stop_streaming() {
    if (!is_streaming_) {
        return false;
    }

    // 处理剩余的音频数据
    if (!audio_buffer_.empty()) {
        try {
            RecognitionConfig config;
            auto result = process_recognition(audio_buffer_, config);
            if (streaming_callback_) {
                streaming_callback_(result);
            }
        } catch (const std::exception& e) {
            std::cerr << "Final streaming recognition error: " << e.what() << std::endl;
        }
    }

    is_streaming_ = false;
    streaming_callback_ = nullptr;
    audio_buffer_.clear();
    state_ = RecognitionState::IDLE;
    return true;
}

std::vector<float> SpeechRecognizer::preprocess_audio(const std::string& audio_path) {
    SF_INFO sf_info;
    std::memset(&sf_info, 0, sizeof(sf_info));
    
    SNDFILE* file = sf_open(audio_path.c_str(), SFM_READ, &sf_info);
    if (!file) {
        throw std::runtime_error("Failed to open audio file: " + audio_path + 
                               " Error: " + sf_strerror(nullptr));
    }

    std::vector<float> buffer(sf_info.frames * sf_info.channels);
    sf_count_t count = sf_read_float(file, buffer.data(), buffer.size());
    sf_close(file);

    if (count != buffer.size()) {
        std::cerr << "Warning: Read fewer frames than expected" << std::endl;
        buffer.resize(count);
    }

    // 如果是多声道，转换为单声道
    if (sf_info.channels > 1) {
        std::vector<float> mono(sf_info.frames);
        for (sf_count_t i = 0; i < sf_info.frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < sf_info.channels; ++ch) {
                sum += buffer[i * sf_info.channels + ch];
            }
            mono[i] = sum / sf_info.channels;
        }
        buffer = std::move(mono);
    }

    // 检查采样率
    if (sf_info.samplerate != 16000) {
        // TODO: 实现重采样
        std::cerr << "Warning: Sample rate is not 16kHz. Resampling not implemented yet." << std::endl;
    }

    return buffer;
}

} // namespace voice_assistant 