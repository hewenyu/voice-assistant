#include "core/speech_recognizer.h"
#include <stdexcept>
#include <sndfile.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>

namespace voice_assistant {

SpeechRecognizer::SpeechRecognizer() 
    : context_(nullptr), initialized_(false), state_(RecognitionState::IDLE) {}

SpeechRecognizer::~SpeechRecognizer() {
    if (context_) {
        sense_voice_free_state(context_->state);
        context_ = nullptr;
    }
}

bool SpeechRecognizer::initialize(const std::string& model_path) {
    try {
        std::cout << "Initializing speech recognizer with model: " << model_path << std::endl;
        
        // 检查模型文件是否存在
        std::ifstream model_file(model_path, std::ios::binary);
        if (!model_file.good()) {
            std::cerr << "Model file not found: " << model_path << std::endl;
            return false;
        }
        model_file.close();

        std::cout << "Debug: Setting up context parameters..." << std::endl;
        // 初始化上下文参数
        sense_voice_context_params params = sense_voice_context_default_params();
        params.use_gpu = false;  // 暂时禁用GPU，看看是否是GPU相关的问题
        params.flash_attn = false;  // 不使用 flash attention
        params.gpu_device = 0;  // 使用第一个 GPU 设备
        
        std::cout << "Debug: Creating context..." << std::endl;
        // 创建新的上下文
        context_ = sense_voice_small_init_from_file_with_params(model_path.c_str(), params);
        if (!context_) {
            std::cerr << "Failed to create sense_voice_context" << std::endl;
            return false;
        }
        std::cout << "Debug: Context created successfully" << std::endl;

        std::cout << "Debug: Checking context members..." << std::endl;
        if (!context_->model.ctx) {
            std::cerr << "Error: Model context is null" << std::endl;
            return false;
        }
        std::cout << "Debug: Model context OK" << std::endl;

        if (!context_->model.buffer) {
            std::cerr << "Error: Model buffer is null" << std::endl;
            return false;
        }
        std::cout << "Debug: Model buffer OK" << std::endl;

        std::cout << "Debug: Checking model type..." << std::endl;
        if (context_->model.model_type.empty()) {
            std::cerr << "Warning: Model type is empty" << std::endl;
        } else {
            std::cout << "Debug: Model type is " << context_->model.model_type << std::endl;
        }

        std::cout << "Debug: Checking vocabulary..." << std::endl;
        if (context_->vocab.n_vocab == 0) {
            std::cerr << "Warning: Vocabulary size is 0" << std::endl;
        } else {
            std::cout << "Debug: Vocabulary size is " << context_->vocab.n_vocab << std::endl;
        }

        std::cout << "Debug: Checking VAD model..." << std::endl;
        if (!context_->vad_model.model) {
            std::cerr << "Warning: VAD model is null" << std::endl;
        } else {
            std::cout << "Debug: VAD model OK" << std::endl;
        }

        std::cout << "Debug: Checking encoder..." << std::endl;
        if (!context_->model.model || !context_->model.model->encoder) {
            std::cerr << "Warning: Encoder is null" << std::endl;
        } else {
            std::cout << "Debug: Encoder OK" << std::endl;
            std::cout << "Debug: Number of encoder layers: " << context_->model.hparams.n_encoder_layers << std::endl;
        }

        std::cout << "Debug: Checking state..." << std::endl;
        if (!context_->state) {
            std::cerr << "Warning: State is null" << std::endl;
        } else {
            std::cout << "Debug: State OK" << std::endl;
        }

        std::cout << "Model loaded successfully" << std::endl;
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in initialize: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception in initialize" << std::endl;
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

    try {
        // 设置识别参数
        sense_voice_full_params params = sense_voice_full_default_params(SENSE_VOICE_SAMPLING_GREEDY);
        params.language = config.language_code.c_str();
        params.n_threads = 1;  // 使用单线程模式
        params.offset_ms = 0;  // 从头开始处理
        params.print_progress = false;  // 不打印进度
        params.progress_callback = nullptr;  // 不使用回调
        params.debug_mode = false;  // 不使用调试模式
        
        // 转换音频数据为double类型
        std::vector<double> samples;
        samples.reserve(audio_data.size());
        for (float sample : audio_data) {
            samples.push_back(static_cast<double>(sample));
        }

        // 确保状态已初始化
        if (!context_->state) {
            // 使用 small 模型的初始化函数
            auto temp_ctx = sense_voice_small_init_from_file_with_params(context_->path_model.c_str(), context_->params);
            if (!temp_ctx || !temp_ctx->state) {
                throw std::runtime_error("Failed to initialize state");
            }
            // 复制状态
            context_->state = temp_ctx->state;
            temp_ctx->state = nullptr;  // 防止被释放
            delete temp_ctx;  // 只删除上下文，不删除状态
        }
        
        // 清空之前的结果
        context_->state->result_all.clear();

        // 分批处理音频数据
        const size_t batch_size = 16000;  // 每批处理1秒的数据
        std::vector<double> batch;
        RecognitionResult final_result;
        
        for (size_t i = 0; i < samples.size(); i += batch_size) {
            // 准备这一批的数据
            size_t current_batch_size = std::min(batch_size, samples.size() - i);
            batch.clear();
            batch.insert(batch.end(), samples.begin() + i, samples.begin() + i + current_batch_size);
            
            std::cout << "Debug: Processing batch " << (i / batch_size + 1) 
                      << " of " << ((samples.size() + batch_size - 1) / batch_size)
                      << " (size: " << current_batch_size << " samples)" << std::endl;
            
            // 执行识别
            int result = sense_voice_full_parallel(
                context_,
                params,
                batch,
                batch.size(),
                1  // 单线程处理
            );

            if (result != 0) {
                throw std::runtime_error("Recognition failed with error code: " + std::to_string(result));
            }

            // 合并结果
            if (context_->state && !context_->state->result_all.empty()) {
                for (const auto& segment : context_->state->result_all) {
                    if (!final_result.transcript.empty()) {
                        final_result.transcript += " ";
                    }
                    final_result.transcript += segment.text;

                    // 如果需要词时间戳
                    if (config.enable_word_time_offsets) {
                        for (const auto& token : segment.tokens) {
                            Word word;
                            word.word = context_->vocab.id_to_token[token.id];
                            word.start_time = token.t0 + (i * 1000.0 / 16000.0);  // 调整时间戳
                            word.end_time = token.t1 + (i * 1000.0 / 16000.0);    // 调整时间戳
                            final_result.words.push_back(word);
                        }
                    }
                }
            }
            
            // 清空这一批的结果，准备处理下一批
            context_->state->result_all.clear();
        }

        final_result.confidence = 1.0;  // sense-voice 目前不提供置信度
        final_result.is_final = true;
        return final_result;
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

    try {
        std::cout << "Starting recognition for file: " << audio_path << std::endl;
        
        // 检查音频文件是否存在
        std::ifstream audio_file(audio_path, std::ios::binary);
        if (!audio_file.good()) {
            std::cerr << "Audio file not found: " << audio_path << std::endl;
            throw std::runtime_error("Audio file not found");
        }

        // 读取音频数据
        auto audio_data = preprocess_audio(audio_path);
        
        // 执行识别
        return process_recognition(audio_data, config);
    } catch (const std::exception& e) {
        std::cerr << "Exception in recognize_sync: " << e.what() << std::endl;
        throw;
    }
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