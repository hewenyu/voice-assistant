#ifndef CORE_RECOGNIZER_SHERPA_RECOGNIZER_H
#define CORE_RECOGNIZER_SHERPA_RECOGNIZER_H

#include "recognizer.h"
#include <sherpa-onnx/c-api/c-api.h>
#include <mutex>

namespace core::recognizer {

class SherpaRecognizer : public IRecognizer {
public:
    SherpaRecognizer() = default;
    ~SherpaRecognizer() override = default;

    bool initialize(const RecognizerConfig& config) override { return true; }
    bool start() override { return true; }
    bool stop() override { return true; }
    bool isRunning() const override { return false; }
    bool feedAudioData(const std::vector<float>& audio_data) override { return true; }
    void setResultCallback(ResultCallback callback) override {}
    std::vector<std::string> getSupportedLanguages() const override { return {}; }

private:
    void cleanup();
    void processRecognitionResult();

    SherpaOnnxOnlineRecognizer* recognizer_;
    SherpaOnnxOnlineStream* stream_;
    SherpaOnnxFeatureExtractor* feature_extractor_;
    
    bool is_initialized_;
    bool is_running_;
    ResultCallback callback_;
    std::mutex mutex_;
    
    // 配置相关
    std::string model_path_;
    std::string lang_;
    int sample_rate_;
    bool enable_vad_;
};

} // namespace core::recognizer

#endif // CORE_RECOGNIZER_SHERPA_RECOGNIZER_H 