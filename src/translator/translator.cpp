#include "translator/translator.h"
#include "translator/deepl/deeplx_translator.h"

namespace translator {

std::unique_ptr<ITranslator> CreateTranslator(TranslatorType type, const common::ModelConfig& config) {
    switch (type) {
        case TranslatorType::DeepLX:
            return std::make_unique<deeplx::DeepLXTranslator>(config);
        case TranslatorType::None:
        default:
            return nullptr;
    }
}

} // namespace translator 