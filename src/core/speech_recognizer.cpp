#include "core/speech_recognizer.h"
#include <stdexcept>
#include <sndfile.h>
#include <iostream>

namespace voice_assistant {

SpeechRecognizer::SpeechRecognizer() : context_(nullptr), initialized_(false) {}

SpeechRecognizer::~SpeechRecognizer() {
    if (context_ && context_->state) {
        sense_voice_free_state(context_->state);
        context_ = nullptr;
    }
}

bool SpeechRecognizer::initialize(const std::string& model_path) {
    try {
        if (context_) {
            if (context_->state) {
                sense_voice_free_state(context_->state);
            }
            context_ = nullptr;
        }

        // 初始化 SenseVoice 模型
        auto params = sense_voice_context_default_params();
        params.use_gpu = true;  // 启用GPU加速
        
        context_ = sense_voice_init_with_params_no_state(
            model_path.c_str(),
            params
        );
        
        if (!context_) {
            std::cerr << "Failed to initialize sense_voice context" << std::endl;
            return false;
        }
        
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing recognizer: " << e.what() << std::endl;
        return false;
    }
}

RecognitionResult SpeechRecognizer::recognize_file(
    const std::string& audio_path,
    const RecognitionConfig& config) {
    
    if (!initialized_) {
        throw std::runtime_error("Recognizer not initialized");
    }

    // 预处理音频
    auto audio_data = preprocess_audio(audio_path);
    return recognize_audio(audio_data, config);
}

RecognitionResult SpeechRecognizer::recognize_audio(
    const std::vector<float>& audio_data,
    const RecognitionConfig& config) {
    
    if (!context_) {
        throw std::runtime_error("Recognizer not initialized");
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
        if (!context_->state->result_all.empty()) {
            const auto& segment = context_->state->result_all[0];
            recognition_result.transcript = segment.text;
            // 由于 API 没有提供置信度，我们暂时设置为 1.0
            recognition_result.confidence = 1.0;
        } else {
            recognition_result.transcript = "";
            recognition_result.confidence = 0.0;
        }

        return recognition_result;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Speech recognition failed: ") + e.what());
    }
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