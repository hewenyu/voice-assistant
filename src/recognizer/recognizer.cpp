// namespace voice
#include "recognizer.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <common/model_config.h>
#include <recognizer/model_factory.h>
#include <translator/deepl/deeplx_translator.h>

namespace recognizer {

Recognizer::Recognizer(const common::ModelConfig& config)
    : recognizer_(nullptr)
    , recognition_stream_(nullptr)
    , vad_(nullptr)
    , recognition_enabled_(false)
    , model_config_(config) {
}

Recognizer::~Recognizer() {
    if (recognition_stream_) {
        SherpaOnnxDestroyOfflineStream(recognition_stream_);
    }
    if (recognizer_) {
        SherpaOnnxDestroyOfflineRecognizer(recognizer_);
    }
    if (vad_) {
        SherpaOnnxDestroyVoiceActivityDetector(vad_);
    }
}

bool Recognizer::initialize() {
    if (!model_config_.vad.model_path.empty()) {
        // Initialize VAD using model_config_
        SherpaOnnxVadModelConfig vad_config = {};
        vad_config.silero_vad.model = model_config_.vad.model_path.c_str();
        vad_config.silero_vad.threshold = model_config_.vad.threshold;
        vad_config.silero_vad.min_silence_duration = model_config_.vad.min_silence_duration;
        vad_config.silero_vad.min_speech_duration = model_config_.vad.min_speech_duration;
        vad_config.silero_vad.max_speech_duration = model_config_.vad.max_speech_duration;
        vad_config.silero_vad.window_size = model_config_.vad.window_size;
        vad_config.sample_rate = model_config_.vad.sample_rate;
        vad_config.num_threads = model_config_.vad.num_threads;
        vad_config.debug = model_config_.vad.debug ? 1 : 0;

        vad_ = SherpaOnnxCreateVoiceActivityDetector(&vad_config, 30);
        if (!vad_) {
            std::cerr << "Failed to create VAD" << std::endl;
            return false;
        }
    }

    try {
        // ModelFactory to create recognizer
        recognizer_ = ModelFactory::CreateModel(model_config_);
        if (!recognizer_) {
            std::cerr << "Failed to create recognizer" << std::endl;
            return false;
        }

        recognition_stream_ = SherpaOnnxCreateOfflineStream(recognizer_);
        if (!recognition_stream_) {
            std::cerr << "Failed to create recognition stream" << std::endl;
            return false;
        }

        recognition_enabled_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize recognition: " << e.what() << std::endl;
        return false;
    }
}

void Recognizer::process_audio(const std::vector<float>& audio_data) {
    if (!recognition_enabled_ || !vad_) {
        return;
    }

    std::lock_guard<std::mutex> lock(recognition_mutex_);

    // If we have remaining samples from last batch, prepend them
    std::vector<float> float_samples;
    if (!remaining_samples_.empty()) {
        float_samples.insert(float_samples.begin(),
                           remaining_samples_.begin(),
                           remaining_samples_.end());
        remaining_samples_.clear();
    }

    // Add new audio data
    float_samples.insert(float_samples.end(), audio_data.begin(), audio_data.end());

    // Process audio in windows of size specified in config
    const int window_size = model_config_.vad.window_size;
    size_t i = 0;
    
    while (i + window_size <= float_samples.size()) {
        // Feed window_size samples to VAD
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(
            vad_,
            float_samples.data() + i,
            window_size
        );

        // Process any complete speech segments
        while (!SherpaOnnxVoiceActivityDetectorEmpty(vad_)) {
            const SherpaOnnxSpeechSegment* segment = 
                SherpaOnnxVoiceActivityDetectorFront(vad_);

            if (segment) {
                process_speech_segment(segment);
                SherpaOnnxDestroySpeechSegment(segment);
            }

            SherpaOnnxVoiceActivityDetectorPop(vad_);
        }

        i += window_size;
    }

    // Store remaining samples for next batch
    if (i < float_samples.size()) {
        remaining_samples_.assign(
            float_samples.begin() + i,
            float_samples.end()
        );
    }
}

void Recognizer::process_speech_segment(const SherpaOnnxSpeechSegment* segment) {
    // Create a new stream for this segment
    const SherpaOnnxOfflineStream* stream = 
        SherpaOnnxCreateOfflineStream(recognizer_);

    if (stream) {
        // Process the speech segment
        SherpaOnnxAcceptWaveformOffline(
            stream,
            model_config_.vad.sample_rate,
            segment->samples,
            segment->n
        );

        SherpaOnnxDecodeOfflineStream(recognizer_, stream);

        const SherpaOnnxOfflineRecognizerResult* result = 
            SherpaOnnxGetOfflineStreamResult(stream);

        if (result && result->text) {
            float start = segment->start / static_cast<float>(model_config_.vad.sample_rate);
            float duration = segment->n / static_cast<float>(model_config_.vad.sample_rate);
            float end = start + duration;

            std::cout << "Time: " << std::fixed << std::setprecision(3)
                      << start << "s -- " << end << "s" << std::endl;
            std::cout << "Text: " << result->text << std::endl;
            
            if (result->lang) {
                std::string language_code = std::string(result->lang).substr(2, 2);
                std::transform(language_code.begin(), language_code.end(), 
                             language_code.begin(), ::toupper);
                std::cout << "Language Code: " << language_code << std::endl;

                std::string target_lang = model_config_.deeplx.target_lang;
                std::transform(target_lang.begin(), target_lang.end(), 
                             target_lang.begin(), ::toupper);
                std::cout << "Target Language: " << target_lang << std::endl;

                if (target_lang != language_code && model_config_.deeplx.enabled) {
                    std::string translated_text = translate(result->text, language_code);
                    std::cout << "Translated Text: " << translated_text << std::endl;
                }
            }
            
            std::cout << std::string(50, '-') << std::endl;
        }

        SherpaOnnxDestroyOfflineRecognizerResult(result);
        SherpaOnnxDestroyOfflineStream(stream);
    }
}

std::string Recognizer::translate(const std::string& text, const std::string& source_lang) {
    if (!model_config_.deeplx.enabled) {
        return text;
    }

    try {
        // Lazy initialization of translator
        if (!translator_) {
            std::string url = model_config_.deeplx.url;
            if (url.find("http://") != 0 && url.find("https://") != 0) {
                url = "http://" + url;
            }
            model_config_.deeplx.url = url;
            // create translator
            translator_ = translator::CreateTranslator(
                translator::TranslatorType::DeepLX,
                model_config_
            );
        }
        return translator_->translate(text, source_lang);
    } catch (const std::exception& e) {
        std::cerr << "Translation error: " << e.what() << std::endl;
        return text;  // Return original text on error
    }
}

} // namespace recognizer
