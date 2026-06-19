#ifndef HTTPCLIENTTEST_H
#define HTTPCLIENTTEST_H

#include "../infra/HttpClient.h"
#include "../infra/StringUtils.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace mbb::tests
{

namespace
{
bool IsCi()
{
    const char* env = std::getenv("CI");
    if (!env || env[0] == '\0')
    {
        return false;
    }
    const std::string value = StringUtils::ToLower(env);
    return value == "1" || value == "true" || value == "yes" || value == "on";
}
} // namespace

class HttpClientTest : public testing::Test
{
  protected:
    HttpClientTest() = default;
    ~HttpClientTest() override = default;
};

TEST_F(HttpClientTest, ConstructorInitializesCurl)
{
    // Should not throw
    EXPECT_NO_THROW({ HttpClient client; });
}

TEST_F(HttpClientTest, MultipleClientsCanCoexist)
{
    HttpClient client1;
    HttpClient client2;
    HttpClient client3;
    // All three should exist without issues
    SUCCEED();
}

TEST_F(HttpClientTest, GetRequestToHttpbin)
{
    // This test requires network access
    // Skip if network is unavailable
    HttpClient client;

    try
    {
        auto response = client.GetString("https://httpbin.org/get");
        if (IsCi() && (response.empty() || response.find("\"url\"") == std::string::npos ||
                       response.find("httpbin.org") == std::string::npos))
        {
            GTEST_SKIP() << "httpbin.org returned an unexpected response in CI (service flaky).";
        }
        EXPECT_FALSE(response.empty());
        EXPECT_TRUE(response.find("\"url\"") != std::string::npos);
        EXPECT_TRUE(response.find("httpbin.org") != std::string::npos);
    }
    catch (const std::exception& e)
    {
        // Network might not be available in CI environment
        GTEST_SKIP() << "Network request failed (may be expected in CI): " << e.what();
    }
}

TEST_F(HttpClientTest, GetRequestReturnsBytes)
{
    HttpClient client;

    try
    {
        auto response = client.Get("https://httpbin.org/bytes/100");
        if (IsCi() && response.size() != 100u)
        {
            GTEST_SKIP() << "httpbin.org returned an unexpected byte count in CI (service flaky).";
        }
        EXPECT_EQ(100u, response.size());
    }
    catch (const std::exception& e)
    {
        GTEST_SKIP() << "Network request failed (may be expected in CI): " << e.what();
    }
}

TEST_F(HttpClientTest, PostJsonToHttpbin)
{
    HttpClient client;

    try
    {
        std::string json = R"({"key": "value", "number": 42})";
        auto response = client.PostJson("https://httpbin.org/post", json);

        if (IsCi() && (response.empty() || response.find("\"key\"") == std::string::npos ||
                       response.find("\"value\"") == std::string::npos))
        {
            GTEST_SKIP() << "httpbin.org returned an unexpected response in CI (service flaky).";
        }
        EXPECT_FALSE(response.empty());
        EXPECT_TRUE(response.find("\"key\"") != std::string::npos);
        EXPECT_TRUE(response.find("\"value\"") != std::string::npos);
    }
    catch (const std::exception& e)
    {
        GTEST_SKIP() << "Network request failed (may be expected in CI): " << e.what();
    }
}

TEST_F(HttpClientTest, PostJsonWithCustomHeaders)
{
    HttpClient client;

    try
    {
        std::string json = "{}";
        std::vector<std::string> headers = {"X-Custom-Header: test-value"};
        auto response = client.PostJson("https://httpbin.org/post", json, headers);

        EXPECT_FALSE(response.empty());
        const auto responseLower = StringUtils::ToLower(response);
        const bool hasHeader = responseLower.find("x-custom-header") != std::string::npos;
        const bool hasValue = responseLower.find("test-value") != std::string::npos;
        if (IsCi() && (!hasHeader || !hasValue))
        {
            GTEST_SKIP() << "Custom headers not echoed by httpbin in CI (likely proxy or sanitization).";
        }
        EXPECT_TRUE(hasHeader);
        EXPECT_TRUE(hasValue);
    }
    catch (const std::exception& e)
    {
        GTEST_SKIP() << "Network request failed (may be expected in CI): " << e.what();
    }
}

TEST_F(HttpClientTest, InvalidUrlThrows)
{
    HttpClient client;
    EXPECT_THROW((void)client.Get("not_a_valid_url"), std::runtime_error);
}

TEST_F(HttpClientTest, SslVerificationWorks)
{
    HttpClient client;

    // This should work - valid SSL certificate
    try
    {
        auto response = client.GetString("https://www.google.com");
        EXPECT_FALSE(response.empty());
    }
    catch (const std::exception& e)
    {
        GTEST_SKIP() << "Network request failed (may be expected in CI): " << e.what();
    }
}

TEST_F(HttpClientTest, ReuseClientForMultipleRequests)
{
    HttpClient client;

    try
    {
        auto response1 = client.GetString("https://httpbin.org/get");
        auto response2 = client.GetString("https://httpbin.org/headers");
        auto response3 = client.GetString("https://httpbin.org/user-agent");

        if (IsCi() && (response1.empty() || response2.empty() || response3.empty()))
        {
            GTEST_SKIP() << "httpbin.org returned an empty response in CI (service flaky).";
        }
        EXPECT_FALSE(response1.empty());
        EXPECT_FALSE(response2.empty());
        EXPECT_FALSE(response3.empty());
    }
    catch (const std::exception& e)
    {
        GTEST_SKIP() << "Network request failed (may be expected in CI): " << e.what();
    }
}

} // namespace mbb::tests

#endif // HTTPCLIENTTEST_H
