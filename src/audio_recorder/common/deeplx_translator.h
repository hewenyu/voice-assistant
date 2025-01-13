#pragma once

#include <string>
#include <memory>
#include <system_error>

namespace voice {

class DeepLXTranslator {
public:
    struct Config {
        std::string url;
        std::string token;
        std::string target_lang;
    };

    explicit DeepLXTranslator(const Config& config);
    ~DeepLXTranslator();

    // Translate text if source language is different from target language
    std::string translate(const std::string& text, const std::string& source_lang);

private:
    struct HttpResponse {
        int status_code;
        std::string body;
    };

    HttpResponse send_post_request(const std::string& json_data);
    bool needs_translation(const std::string& source_lang) const;
    std::string make_http_request(const std::string& host, int port, 
                                const std::string& path, const std::string& body);

    Config config_;
    std::string host_;
    std::string path_;
    int port_;
};

} // namespace voice 