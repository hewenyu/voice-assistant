#include <gtest/gtest.h>
#include "../src/core/translator/translator.h"
#include <thread>
#include <chrono>
#include <future>
#include <algorithm>

using namespace core::translator;

class TranslatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        translator = createTranslator();
        ASSERT_NE(translator, nullptr);

        // 设置基本配置
        config.api_endpoint = "http://localhost:1188";  // DeepLX 默认端点
        config.timeout_ms = 5000;
        config.use_proxy = false;
    }

    void TearDown() override {
        translator.reset();
    }

    std::unique_ptr<ITranslator> translator;
    TranslatorConfig config;
};

// 测试初始化
TEST_F(TranslatorTest, Initialize) {
    EXPECT_TRUE(translator->initialize(config));
}

// 测试基本翻译功能
TEST_F(TranslatorTest, BasicTranslation) {
    ASSERT_TRUE(translator->initialize(config));

    TranslationResult result;
    bool success = translator->translate("Hello, world!", "en", "zh", result);

    EXPECT_TRUE(success);
    EXPECT_FALSE(result.translated_text.empty());
    EXPECT_EQ(result.source_lang, "en");
    EXPECT_EQ(result.target_lang, "zh");
    EXPECT_GT(result.confidence, 0.0f);
}

// 测试异步翻译
TEST_F(TranslatorTest, AsyncTranslation) {
    ASSERT_TRUE(translator->initialize(config));

    std::promise<TranslationResult> promise;
    auto future = promise.get_future();

    translator->translateAsync(
        "Hello, world!", "en", "zh",
        [&promise](const TranslationResult& result) {
            promise.set_value(result);
        }
    );

    // 等待结果（设置超时）
    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    if (status == std::future_status::ready) {
        auto result = future.get();
        EXPECT_FALSE(result.translated_text.empty());
        EXPECT_EQ(result.source_lang, "en");
        EXPECT_EQ(result.target_lang, "zh");
    }
}

// 测试语言检测
TEST_F(TranslatorTest, LanguageDetection) {
    ASSERT_TRUE(translator->initialize(config));

    // 测试英文
    EXPECT_EQ(translator->detectLanguage("Hello, world!"), "en");

    // 测试中文
    EXPECT_EQ(translator->detectLanguage("你好，世界！"), "zh");

    // 测试日文
    EXPECT_EQ(translator->detectLanguage("こんにちは、世界！"), "ja");
}

// 测试支持的语言
TEST_F(TranslatorTest, SupportedLanguages) {
    auto languages = translator->getSupportedLanguages();
    EXPECT_FALSE(languages.empty());

    // 验证基本语言是否支持
    auto has_language = [&languages](const std::string& lang) {
        return std::find(languages.begin(), languages.end(), lang) != languages.end();
    };

    EXPECT_TRUE(has_language("en"));
    EXPECT_TRUE(has_language("zh"));
    EXPECT_TRUE(has_language("ja"));
}

// 测试语言对支持
TEST_F(TranslatorTest, LanguagePairSupport) {
    EXPECT_TRUE(translator->isLanguagePairSupported("en", "zh"));
    EXPECT_TRUE(translator->isLanguagePairSupported("zh", "en"));
    EXPECT_TRUE(translator->isLanguagePairSupported("ja", "zh"));
}

// 测试错误处理
TEST_F(TranslatorTest, ErrorHandling) {
    ASSERT_TRUE(translator->initialize(config));

    // 测试无效的语言对
    TranslationResult result;
    EXPECT_FALSE(translator->translate("Hello", "invalid", "zh", result));

    // 测试空文本
    EXPECT_FALSE(translator->translate("", "en", "zh", result));

    // 测试无效的API端点
    TranslatorConfig invalid_config = config;
    invalid_config.api_endpoint = "http://invalid.endpoint";
    EXPECT_FALSE(translator->initialize(invalid_config));
}

// 测试长文本翻译
TEST_F(TranslatorTest, LongTextTranslation) {
    ASSERT_TRUE(translator->initialize(config));

    // 创建一个长文本
    std::string long_text;
    for (int i = 0; i < 10; ++i) {
        long_text += "This is a long text that needs to be translated. ";
    }

    TranslationResult result;
    bool success = translator->translate(long_text, "en", "zh", result);

    EXPECT_TRUE(success);
    EXPECT_FALSE(result.translated_text.empty());
    EXPECT_GT(result.translated_text.length(), long_text.length() / 4);  // 考虑到中文字符通常更短
}

// 测试并发翻译
TEST_F(TranslatorTest, ConcurrentTranslation) {
    ASSERT_TRUE(translator->initialize(config));

    const int num_threads = 5;
    std::vector<std::future<bool>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [this]() {
            TranslationResult result;
            return translator->translate("Hello, world!", "en", "zh", result);
        }));
    }

    // 等待所有翻译完成
    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }
}

// 测试代理设置
TEST_F(TranslatorTest, ProxySettings) {
    // 设置代理配置
    TranslatorConfig proxy_config = config;
    proxy_config.use_proxy = true;
    proxy_config.proxy_url = "http://localhost:8080";

    ASSERT_TRUE(translator->initialize(proxy_config));

    TranslationResult result;
    bool success = translator->translate("Hello, world!", "en", "zh", result);

    // 注意：这个测试可能会失败，取决于代理是否实际可用
    if (success) {
        EXPECT_FALSE(result.translated_text.empty());
    }
} 