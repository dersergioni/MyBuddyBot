#ifndef INTEGRATIONTEST_H
#define INTEGRATIONTEST_H

#include "../bot/BotApp.h"
#include "../core/Config.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>

#define FUNC_PROLOGUE std::cout << __func__ << std::endl

namespace mbb::tests
{

namespace
{
bool HasEnv(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

std::string NormalizeEnvValue(const char* value)
{
    std::string normalized = value ? value : "";
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

bool IsTruthy(const char* value)
{
    const auto normalized = NormalizeEnvValue(value);
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

bool ShouldRunIntegrationTests()
{
    const char* runFlag = std::getenv("MYBUDDYBOT_RUN_INTEGRATION_TESTS");
    if (runFlag != nullptr)
    {
        return IsTruthy(runFlag);
    }

    const char* ci = std::getenv("CI");
    if (IsTruthy(ci))
    {
        return false;
    }

    return true;
}

bool HasRequiredIntegrationEnv()
{
    return HasEnv("MYBUDDYBOT_DB_PATH") && HasEnv("TG_API_TOKEN") && HasEnv("OAI_API_TOKEN") &&
           HasEnv("XAI_API_TOKEN") && HasEnv("GOOGLE_API_TOKEN");
}
} // namespace

class IntegrationTest : public testing::Test
{
  protected:
    BotApp app;
    std::future<int> futureApp;

    IntegrationTest()
    {
        FUNC_PROLOGUE;
        Config::SetTestMode(true);
    }

    ~IntegrationTest() override
    {
        FUNC_PROLOGUE;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void SetUp() override
    {
        FUNC_PROLOGUE;
        if (!ShouldRunIntegrationTests())
        {
            GTEST_SKIP() << "Integration tests disabled. Set MYBUDDYBOT_RUN_INTEGRATION_TESTS=1 to enable.";
        }
        if (!HasRequiredIntegrationEnv())
        {
            GTEST_SKIP() << "Integration tests require MYBUDDYBOT_DB_PATH, TG_API_TOKEN, OAI_API_TOKEN, "
                            "XAI_API_TOKEN, GOOGLE_API_TOKEN.";
        }
        futureApp = std::async(std::launch::async, [&]() { return app.Run(); });
    }

    void TearDown() override
    {
        FUNC_PROLOGUE;
    }

    void StopApp()
    {
        FUNC_PROLOGUE;
        app.Stop();
    }

    std::optional<int> WaitForApp(int32_t sec)
    {
        FUNC_PROLOGUE;
        while (sec-- > 0)
        {
            std::future_status status = futureApp.wait_for(std::chrono::seconds(1));
            if (status == std::future_status::timeout)
            {
                std::cout << "The Application is still running. Waiting..." << std::endl;
            }
            else if (status == std::future_status::deferred)
            {
                std::cout << "Should be here" << std::endl;
                return EXIT_FAILURE;
            }
            else if (status == std::future_status::ready)
            {
                const auto res = futureApp.get();
                std::cout << "The Application has finished with " << res << " code" << std::endl;
                return res;
            }
        }
        return std::nullopt;
    }

  public:
};

TEST_F(IntegrationTest, RunAppWithStop)
{
    std::this_thread::sleep_for(std::chrono::seconds(5));

    StopApp();
    const auto res = WaitForApp(10);

    ASSERT_TRUE(res.has_value()) << "The Application doesn't start/stop properly";
    ASSERT_EQ(0, res) << "The Application doesn't start/stop properly";
}

TEST_F(IntegrationTest, RunAppWithoutStop)
{
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto res = WaitForApp(10);
    ASSERT_FALSE(res.has_value()) << "The Application doesn't start/stop properly";
    std::cout << "The Application is still running and working as expected" << std::endl;
    StopApp();
    res = WaitForApp(10);
    ASSERT_TRUE(res.has_value()) << "The Application doesn't start/stop properly";
    ASSERT_EQ(0, res) << "The Application doesn't start/stop properly";
}
} // namespace mbb::tests

#endif // INTEGRATIONTEST_H
