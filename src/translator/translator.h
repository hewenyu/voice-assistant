// 定义翻译类型
#pragma once


// 翻译器命名空间
namespace translator {
    // 翻译器类型
    enum class TranslatorType {
        DeepLX,
        Google,
        Microsoft,
        None
    };

    
    // ITranslator
    class ITranslator {
    public:
        virtual ~ITranslator() = default;
        virtual std::string translate(const std::string& text, const std::string& source_lang) = 0;
    };

    // 创建翻译器
    std::unique_ptr<ITranslator> CreateTranslator(TranslatorType type, const std::string& config_path);    
}


