#ifndef CORE_TRANSLATOR_H
#define CORE_TRANSLATOR_H

#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace core {
namespace translator {

// 翻译结果结构体
struct TranslationResult {
    std::string source_text;      // 源文本
    std::string translated_text;   // 翻译后的文本
    std::string source_lang;      // 源语言
    std::string target_lang;      // 目标语言
    float confidence;             // 置信度
};

// 翻译器配置结构体
struct TranslatorConfig {
    std::string api_key;          // API密钥（如果需要）
    std::string api_endpoint;     // API端点
    bool use_proxy;               // 是否使用代理
    std::string proxy_url;        // 代理URL
    int timeout_ms;               // 超时时间（毫秒）
};

// 翻译器接口类
class ITranslator {
public:
    virtual ~ITranslator() = default;

    // 初始化翻译器
    virtual bool initialize(const TranslatorConfig& config) = 0;

    // 翻译文本
    virtual bool translate(const std::string& text, 
                         const std::string& from_lang,
                         const std::string& to_lang,
                         TranslationResult& result) = 0;

    // 异步翻译文本
    virtual void translateAsync(const std::string& text,
                              const std::string& from_lang,
                              const std::string& to_lang,
                              std::function<void(const TranslationResult&)> callback) = 0;

    // 检测语言
    virtual std::string detectLanguage(const std::string& text) = 0;

    // 获取支持的语言列表
    virtual std::vector<std::string> getSupportedLanguages() const = 0;

    // 获取语言对是否支持
    virtual bool isLanguagePairSupported(const std::string& from_lang,
                                       const std::string& to_lang) const = 0;
};

// 创建翻译器实例
std::unique_ptr<ITranslator> createTranslator();

} // namespace translator
} // namespace core

#endif // CORE_TRANSLATOR_H 