// 翻译器 工厂类函数
#include "translator.h"
#include <memory>   // Add this line for std::unique_ptr
#include "common/model_config.h"
#include "deepl/deeplx_translator.h"

namespace translator {
    // 创建翻译器 根据类型
    std::unique_ptr<ITranslator> CreateTranslator(TranslatorType type, const common::ModelConfig& config) {
        switch (type) {
            case TranslatorType::DeepLX:
                return std::make_unique<deeplx::DeepLXTranslator>(config);
            case TranslatorType::None:
                return nullptr;
        }
    }
}
