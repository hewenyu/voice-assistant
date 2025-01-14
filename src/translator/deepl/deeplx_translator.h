#pragma once

#include <string>
#include <memory>
#include <curl/curl.h>
#include "common/model_config.h"
#include "translator/translator.h"

namespace deeplx {

class DeepLXTranslator : public translator::ITranslator {
public:
    explicit DeepLXTranslator(const common::ModelConfig& config);
    ~DeepLXTranslator() override;

    std::string translate(const std::string& text, const std::string& source_lang) override;

private:
    struct HttpResponse {
        int status_code;
        std::string body;
    };

    bool needs_translation(const std::string& source_lang) const;
    HttpResponse send_post_request(const std::string& json_data);
    std::string make_http_request(const std::string& host, int port, 
                                const std::string& path, const std::string& data);

    std::string url_;
    std::string token_;
    std::string target_lang_;
    std::string host_;
    std::string path_;
    int port_;
    CURL* curl_;
};

} // namespace deeplx 