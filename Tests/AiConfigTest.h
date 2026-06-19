#ifndef AICONFIGTEST_H
#define AICONFIGTEST_H

#include "../ai/IAiService.h"
#include "../core/AiConfig.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace mbb::tests
{

namespace
{
const char* kSampleConfig = R"({
  "providers": {
    "openai": {
      "apiKey": "sk-openai",
      "models": {
        "primary": { "name": "gpt-x", "url": "https://example.test/openai", "contextSize": 123456 },
        "image": { "name": "img-x" }
      }
    },
    "google": {
      "apiKey": "g-key"
    }
  }
})";
}

TEST(AiConfigTest, SpecIsCompleteOnlyWithAllFields)
{
    AiModelSpec spec;
    EXPECT_FALSE(spec.IsComplete());
    spec.name = "n";
    EXPECT_FALSE(spec.IsComplete());
    spec.url = "u";
    EXPECT_FALSE(spec.IsComplete());
    spec.contextSize = 1;
    EXPECT_TRUE(spec.IsComplete());
}

TEST(AiConfigTest, EmptyConfigYieldsNoKeysOrSpecs)
{
    AiConfig config;
    EXPECT_FALSE(config.GetApiKey("openai").has_value());
    EXPECT_FALSE(config.GetModelSpec("openai", "primary").IsComplete());
}

TEST(AiConfigTest, ParsesApiKeysPerProvider)
{
    AiConfig config;
    config.LoadFromString(kSampleConfig);

    ASSERT_TRUE(config.GetApiKey("openai").has_value());
    EXPECT_EQ("sk-openai", *config.GetApiKey("openai"));
    ASSERT_TRUE(config.GetApiKey("google").has_value());
    EXPECT_EQ("g-key", *config.GetApiKey("google"));
    EXPECT_FALSE(config.GetApiKey("xai").has_value());
}

TEST(AiConfigTest, ParsesCompleteModelSpec)
{
    AiConfig config;
    config.LoadFromString(kSampleConfig);

    const AiModelSpec spec = config.GetModelSpec("openai", "primary");
    ASSERT_TRUE(spec.IsComplete());
    EXPECT_EQ("gpt-x", *spec.name);
    EXPECT_EQ("https://example.test/openai", *spec.url);
    EXPECT_EQ(123456u, *spec.contextSize);
}

TEST(AiConfigTest, PartialModelSpecIsIncomplete)
{
    AiConfig config;
    config.LoadFromString(kSampleConfig);

    const AiModelSpec spec = config.GetModelSpec("openai", "image");
    EXPECT_FALSE(spec.IsComplete());
    ASSERT_TRUE(spec.name.has_value());
    EXPECT_EQ("img-x", *spec.name);
    EXPECT_FALSE(spec.url.has_value());
}

TEST(AiConfigTest, MissingProviderOrSlotYieldsIncompleteSpec)
{
    AiConfig config;
    config.LoadFromString(kSampleConfig);

    EXPECT_FALSE(config.GetModelSpec("openai", "audio").IsComplete());
    EXPECT_FALSE(config.GetModelSpec("xai", "primary").IsComplete());
}

TEST(AiConfigTest, ProviderAndSlotNamesAreCaseInsensitive)
{
    AiConfig config;
    config.LoadFromString(
        R"({"providers": {"OpenAI": {"models": {"Primary": {"name": "n", "url": "u", "contextSize": 5}}}}})");

    const AiModelSpec spec = config.GetModelSpec("openai", "primary");
    ASSERT_TRUE(spec.IsComplete());
    EXPECT_EQ("n", *spec.name);
}

TEST(AiConfigTest, MalformedJsonThrows)
{
    AiConfig config;
    EXPECT_THROW(config.LoadFromString("{ this is not json"), std::runtime_error);
}

TEST(AiConfigTest, MissingProvidersObjectThrows)
{
    AiConfig config;
    EXPECT_THROW(config.LoadFromString(R"({"something": 1})"), std::runtime_error);
}

TEST(AiConfigTest, StripsLeadingUtf8Bom)
{
    AiConfig config;
    const std::string withBom = std::string("\xEF\xBB\xBF") + R"({"providers": {"openai": {"apiKey": "bom-key"}}})";
    config.LoadFromString(withBom);

    ASSERT_TRUE(config.GetApiKey("openai").has_value());
    EXPECT_EQ("bom-key", *config.GetApiKey("openai"));
}

TEST(AiConfigTest, ParsesZeroContextSizeAsComplete)
{
    AiConfig config;
    config.LoadFromString(
        R"({"providers": {"openai": {"models": {"image": {"name": "n", "url": "u", "contextSize": 0}}}}})");

    const AiModelSpec spec = config.GetModelSpec("openai", "image");
    ASSERT_TRUE(spec.IsComplete());
    EXPECT_EQ(0u, *spec.contextSize);
}

TEST(AiConfigTest, IgnoresNonIntegerContextSize)
{
    AiConfig config;
    config.LoadFromString(
        R"({"providers": {"openai": {"models": {"primary": {"name": "n", "url": "u", "contextSize": 1.5}}}}})");

    const AiModelSpec spec = config.GetModelSpec("openai", "primary");
    EXPECT_FALSE(spec.contextSize.has_value());
    EXPECT_FALSE(spec.IsComplete());
}

TEST(AiConfigTest, BuildModelFromCompleteSpec)
{
    AiModelSpec spec;
    spec.name = "model-x";
    spec.url = "https://url";
    spec.contextSize = 4096;

    const AiModel model = BuildModel(spec, "secret-key");
    EXPECT_EQ("model-x", model.name);
    EXPECT_EQ("https://url", model.url);
    EXPECT_EQ(4096u, model.contextSize);
    EXPECT_EQ("secret-key", model.apiKey);
}

TEST(AiConfigTest, BuildModelFromIncompleteSpecIsEmpty)
{
    AiModelSpec spec;
    spec.name = "model-x"; // url and contextSize missing

    const AiModel model = BuildModel(spec, "secret-key");
    EXPECT_TRUE(model.name.empty());
    EXPECT_TRUE(model.url.empty());
    EXPECT_EQ(0u, model.contextSize);
    EXPECT_TRUE(model.apiKey.empty());
}

} // namespace mbb::tests

#endif // AICONFIGTEST_H
