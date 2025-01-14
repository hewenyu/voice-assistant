#pragma once

#include <memory>
#include <string>
#include "common/model_config.h"

namespace deeplx {
class DeepLXTranslator;
}

namespace translator {

enum class TranslatorType {
    DeepLX,
    Google,
    Microsoft,
    None
};

class ITranslator {
public:
    virtual ~ITranslator() = default;
    virtual std::string translate(const std::string& text, const std::string& source_lang) = 0;
};

// Factory function to create translator
std::unique_ptr<ITranslator> CreateTranslator(TranslatorType type, const common::ModelConfig& config);

} // namespace translator


