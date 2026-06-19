#ifndef AISERVICESSETEST_H
#define AISERVICESSETEST_H

#include "../ai/IAiService.h"
#include "../telegram/IMessageWorker.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mbb::tests
{

class NoopMessageWorker final : public IMessageWorker
{
  public:
    void Start(std::shared_ptr<TelegramApi> /*api*/) override
    {
    }

    void Stop() override
    {
    }

    std::optional<uint32_t> AddMessagePortion(std::optional<uint32_t> id,
                                              int64_t /*chatId*/,
                                              int32_t /*threadId*/,
                                              const std::string& /*responseText*/) override
    {
        return id;
    }

    void FinalizeMessage(std::optional<uint32_t> /*id*/) override
    {
    }
};

class TestAiService : public IAiService
{
  public:
    using IAiService::IsQuotaError;
    using IAiService::StreamState;
    using IAiService::TextStreamCallback;

    mutable std::vector<std::string> chunks;

    explicit TestAiService(std::shared_ptr<IMessageWorker> worker = std::make_shared<NoopMessageWorker>())
        : IAiService(std::move(worker))
    {
    }
    ~TestAiService() override = default;

    [[nodiscard]] AiResponse GetTextResponse(int64_t /*chatId*/,
                                             int32_t /*threadId*/,
                                             const std::vector<std::pair<std::string, std::string>>& /*history*/,
                                             const std::vector<std::string>& /*visionImagesBase64*/,
                                             ModelSelector /*model*/,
                                             bool /*returnAudio*/) override
    {
        return {};
    }

    [[nodiscard]] AiResponse GetResponseFromVoice(int64_t /*chatId*/,
                                                  int32_t /*threadId*/,
                                                  const std::string& /*audioBase64*/,
                                                  bool /*returnAudio*/) override
    {
        return {};
    }

    [[nodiscard]] AiResponse GetImageResponse(int64_t /*chatId*/,
                                              int32_t /*threadId*/,
                                              const std::string& /*prompt*/,
                                              const std::vector<std::string>& /*referenceImagesBase64*/) override
    {
        return {};
    }

  protected:
    [[nodiscard]] bool ExtractStreamContent(const std::string& jsonChunk, StreamState& /*state*/) const override
    {
        chunks.push_back(jsonChunk);
        return false;
    }
};

TEST(AiServiceSseTest, ParsesSplitEventsAcrossCallbacks)
{
    TestAiService service;
    TestAiService::StreamState state{};
    state.service = &service;

    const std::string part1 = "data: {\"foo\":1";
    const std::string part2 = "}\n\n";

    TestAiService::TextStreamCallback(const_cast<char*>(part1.data()), 1, part1.size(), &state);
    EXPECT_TRUE(service.chunks.empty());

    TestAiService::TextStreamCallback(const_cast<char*>(part2.data()), 1, part2.size(), &state);
    ASSERT_EQ(1u, service.chunks.size());
    EXPECT_EQ("{\"foo\":1}", service.chunks[0]);
}

TEST(AiServiceSseTest, IgnoresDoneEvents)
{
    TestAiService service;
    TestAiService::StreamState state{};
    state.service = &service;

    const std::string chunk = "data: [DONE]\n\n";
    TestAiService::TextStreamCallback(const_cast<char*>(chunk.data()), 1, chunk.size(), &state);

    EXPECT_TRUE(service.chunks.empty());
}

TEST(AiServiceSseTest, HandlesCrlfEventSeparators)
{
    TestAiService service;
    TestAiService::StreamState state{};
    state.service = &service;

    const std::string chunk = "data: {\"foo\":2}\r\n\r\n";
    TestAiService::TextStreamCallback(const_cast<char*>(chunk.data()), 1, chunk.size(), &state);

    ASSERT_EQ(1u, service.chunks.size());
    EXPECT_EQ("{\"foo\":2}", service.chunks[0]);
}

TEST(AiServiceSseTest, ConcatenatesMultipleDataLines)
{
    TestAiService service;
    TestAiService::StreamState state{};
    state.service = &service;

    const std::string chunk = "data: {\"foo\":\n"
                              "data: 3}\n\n";
    TestAiService::TextStreamCallback(const_cast<char*>(chunk.data()), 1, chunk.size(), &state);

    ASSERT_EQ(1u, service.chunks.size());
    EXPECT_EQ("{\"foo\":\n3}", service.chunks[0]);
}

TEST(AiServiceSseTest, IgnoresEventLinesAndParsesData)
{
    TestAiService service;
    TestAiService::StreamState state{};
    state.service = &service;

    const std::string chunk = "event: response.output_text.delta\n"
                              "data: {\"foo\":4}\n\n";
    TestAiService::TextStreamCallback(const_cast<char*>(chunk.data()), 1, chunk.size(), &state);

    ASSERT_EQ(1u, service.chunks.size());
    EXPECT_EQ("{\"foo\":4}", service.chunks[0]);
}

TEST(AiServiceQuotaTest, DetectsOpenAiInsufficientQuotaByCode)
{
    EXPECT_TRUE(TestAiService::IsQuotaError("insufficient_quota", "You exceeded your current quota"));
}

TEST(AiServiceQuotaTest, DetectsQuotaByMessageOnly)
{
    EXPECT_TRUE(TestAiService::IsQuotaError("", "You exceeded your current quota, please check your plan and billing"));
}

TEST(AiServiceQuotaTest, DetectsGeminiResourceExhaustedByStatusCode)
{
    EXPECT_TRUE(TestAiService::IsQuotaError("RESOURCE_EXHAUSTED", "Quota exceeded for metric"));
}

TEST(AiServiceQuotaTest, DetectsBillingCode)
{
    EXPECT_TRUE(TestAiService::IsQuotaError("billing_hard_limit_reached", ""));
}

TEST(AiServiceQuotaTest, IgnoresTransientAndUnrelatedErrors)
{
    EXPECT_FALSE(TestAiService::IsQuotaError("rate_limit_exceeded", "Rate limit reached, please slow down"));
    EXPECT_FALSE(TestAiService::IsQuotaError("server_error", "The server had an error processing your request"));
    EXPECT_FALSE(TestAiService::IsQuotaError("invalid_api_key", "Incorrect API key provided"));
    EXPECT_FALSE(TestAiService::IsQuotaError("", ""));
}

TEST(AiServiceQuotaTest, StreamErrorEventRecordsQuotaFlag)
{
    TestAiService service;
    TestAiService::StreamState state{};
    state.service = &service;

    const std::string chunk =
        "data: {\"error\":{\"code\":\"insufficient_quota\",\"message\":\"You exceeded your current quota\"}}\n\n";
    TestAiService::TextStreamCallback(const_cast<char*>(chunk.data()), 1, chunk.size(), &state);

    ASSERT_TRUE(state.streamError.has_value());
    EXPECT_TRUE(state.streamErrorIsQuota);
}

TEST(AiServiceQuotaTest, StreamErrorWithoutQuotaSignatureIsNotQuota)
{
    TestAiService service;
    TestAiService::StreamState state{};
    state.service = &service;

    const std::string chunk = "data: {\"error\":{\"type\":\"server_error\",\"message\":\"Internal failure\"}}\n\n";
    TestAiService::TextStreamCallback(const_cast<char*>(chunk.data()), 1, chunk.size(), &state);

    ASSERT_TRUE(state.streamError.has_value());
    EXPECT_FALSE(state.streamErrorIsQuota);
}

} // namespace mbb::tests

#endif // AISERVICESSETEST_H
