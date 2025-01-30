#ifndef CORE_TRANSLATOR_DEEPLX_TRANSLATOR_H
#define CORE_TRANSLATOR_DEEPLX_TRANSLATOR_H

#include "translator.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>

namespace core::translator {

// 异步翻译请求结构体
struct TranslationRequest {
    std::string text;
    std::string from_lang;
    std::string to_lang;
    std::function<void(const TranslationResult&)> callback;
};

class DeepLXTranslator : public ITranslator {
public:
    DeepLXTranslator() = default;
    ~DeepLXTranslator() override = default;

    bool initialize(const TranslatorConfig& config) override { return true; }
    bool translate(const std::string& text, const std::string& source_lang, 
                  const std::string& target_lang, TranslationResult& result) override { return true; }
    void translateAsync(const std::string& text, const std::string& source_lang,
                       const std::string& target_lang, ResultCallback callback) override {}
    std::string detectLanguage(const std::string& text) override { return ""; }
    std::vector<std::string> getSupportedLanguages() const override { return {}; }
    bool isLanguagePairSupported(const std::string& source_lang, 
                                const std::string& target_lang) const override { return true; }

private:
    void cleanup();
    void workerThread();
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    bool sendRequest(const std::string& url, const nlohmann::json& data, std::string& response);
    bool parseResponse(const std::string& response, TranslationResult& result);

    CURL* curl_;
    std::string api_endpoint_;
    std::string api_key_;
    bool use_proxy_;
    std::string proxy_url_;
    int timeout_ms_;

    // 异步处理相关
    std::thread worker_thread_;
    std::queue<TranslationRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    bool should_stop_;

    // 支持的语言对缓存
    std::vector<std::pair<std::string, std::string>> supported_pairs_;
    mutable std::mutex pairs_mutex_;
};

} // namespace core::translator

#endif // CORE_TRANSLATOR_DEEPLX_TRANSLATOR_H 