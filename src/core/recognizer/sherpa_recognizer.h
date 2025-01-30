#ifndef CORE_SHERPA_RECOGNIZER_H
#define CORE_SHERPA_RECOGNIZER_H

#include "recognizer.h"
#include <sherpa-onnx/c-api/c-api.h>
#include <mutex>

namespace core {
namespace recognizer {

class SherpaRecognizer : public IRecognizer {
public:
    SherpaRecognizer();
    ~SherpaRecognizer() override;

    bool initialize(const RecognizerConfig& config) override;
    bool start() override;
    void stop() override;
    void reset() override;
    bool feedAudioData(const float* audio_data, int num_samples) override;
    void setResultCallback(std::function<void(const RecognitionResult&)> callback) override;
    std::vector<std::string> getSupportedLanguages() const override;

private:
    void cleanup();
    void processRecognitionResult();

    SherpaOnnxOnlineRecognizer* recognizer_;
    SherpaOnnxOnlineStream* stream_;
    SherpaOnnxFeatureExtractor* feature_extractor_;
    
    bool is_initialized_;
    bool is_running_;
    std::function<void(const RecognitionResult&)> callback_;
    std::mutex mutex_;
    
    // 配置相关
    std::string model_path_;
    std::string lang_;
    int sample_rate_;
    bool enable_vad_;
};

} // namespace recognizer
} // namespace core

#endif // CORE_SHERPA_RECOGNIZER_H 