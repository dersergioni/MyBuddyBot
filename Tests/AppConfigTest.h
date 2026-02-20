#ifndef APPCONFIGTEST_H
#define APPCONFIGTEST_H

#include "../core/Config.h"

#include <gtest/gtest.h>

#include <clocale>
#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#endif

#define FUNC_PROLOGUE std::cout << __func__ << std::endl

namespace mbb::tests
{

class ConfigTest : public testing::Test
{
  protected:
    ConfigTest()
    {
        FUNC_PROLOGUE;
#if defined(_WIN32)
        SetConsoleOutputCP(65001);
        setlocale(LC_ALL, ".UTF-8");
#endif

        Config::SetTestMode(true);
    }

    ~ConfigTest() override
    {
        FUNC_PROLOGUE;
    }

    void SetUp() override
    {
        FUNC_PROLOGUE;
    }

    void TearDown() override
    {
        FUNC_PROLOGUE;
    }

  public:
};

TEST_F(ConfigTest, InitConfig)
{
    const char* envDebugLevelMode = getenv("MYBUDDYBOT_DEBUG_LEVEL_MODE");
    const char* envDbPath = getenv("MYBUDDYBOT_DB_PATH");
    const char* envBotToken = getenv("TG_API_TOKEN");
    const char* envOpenAiToken = getenv("OAI_API_TOKEN");
    const char* envXAiToken = getenv("XAI_API_TOKEN");
    const char* envGoogleToken = getenv("GOOGLE_API_TOKEN");
    const char* envDefaultProvider = getenv("MYBUDDYBOT_DEFAULT_PROVIDER");
    const char* envStatePath = getenv("MYBUDDYBOT_STATE_PATH");
    const char* envBotName = getenv("MYBUDDYBOT_NAME");

#if defined(_WIN32)
    _putenv_s("MYBUDDYBOT_DEBUG_LEVEL_MODE", "1");
    _putenv_s("MYBUDDYBOT_DB_PATH", "C:\\DB\\Проект\\MyBuddy.db");
    _putenv_s("TG_API_TOKEN", "1234567890:ABC");
    _putenv_s("OAI_API_TOKEN", "ABC123");
    _putenv_s("XAI_API_TOKEN", "");
    _putenv_s("GOOGLE_API_TOKEN", "");
    _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", "openai");
    _putenv_s("MYBUDDYBOT_STATE_PATH", "C:\\DB\\Проект\\MyBuddy.state");
    _putenv_s("MYBUDDYBOT_NAME", "MyBuddyDevBot");
#else
    setenv("MYBUDDYBOT_DEBUG_LEVEL_MODE", "1", 1);
    setenv("MYBUDDYBOT_DB_PATH", "/root/var/db/mybuddy.db", 1);
    setenv("TG_API_TOKEN", "1234567890:ABC", 1);
    setenv("OAI_API_TOKEN", "ABC123", 1);
    setenv("XAI_API_TOKEN", "", 1);
    setenv("GOOGLE_API_TOKEN", "", 1);
    setenv("MYBUDDYBOT_DEFAULT_PROVIDER", "openai", 1);
    setenv("MYBUDDYBOT_STATE_PATH", "/root/var/db/mybuddy.state", 1);
    setenv("MYBUDDYBOT_NAME", "MyBuddyDevBot", 1);
#endif

    Config::Init();

    ASSERT_TRUE(Config::IsDebugMode());
#if defined(_WIN32)
    ASSERT_EQ("C:\\DB\\Проект\\MyBuddy.db", Config::GetDbPath());
    ASSERT_EQ(std::filesystem::path("C:\\DB\\Проект\\MyBuddy.state"), Config::GetStatePath());
#else
    ASSERT_EQ("/root/var/db/mybuddy.db", Config::GetDbPath());
    ASSERT_EQ(std::filesystem::path("/root/var/db/mybuddy.state"), Config::GetStatePath());
#endif
    ASSERT_EQ("1234567890:ABC", Config::GetBotToken());
    ASSERT_EQ("ABC123", Config::GetOpenAiToken());
    ASSERT_EQ("MyBuddyDevBot", Config::GetBotName());
    ASSERT_EQ(AiProvider::OpenAI, Config::GetDefaultProvider());
    ASSERT_TRUE(Config::IsProviderEnabled(AiProvider::OpenAI));
    ASSERT_FALSE(Config::IsProviderEnabled(AiProvider::XAI));
    ASSERT_FALSE(Config::IsProviderEnabled(AiProvider::Google));
    ASSERT_EQ(1u, Config::GetEnabledProviders().size());

#if defined(_WIN32)
    if (envDebugLevelMode != nullptr)
    {
        _putenv_s("MYBUDDYBOT_DEBUG_LEVEL_MODE", envDebugLevelMode);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_DEBUG_LEVEL_MODE", "");
    }
    if (envDbPath != nullptr)
    {
        _putenv_s("MYBUDDYBOT_DB_PATH", envDbPath);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_DB_PATH", "");
    }
    if (envBotToken != nullptr)
    {
        _putenv_s("TG_API_TOKEN", envBotToken);
    }
    else
    {
        _putenv_s("TG_API_TOKEN", "");
    }
    if (envOpenAiToken != nullptr)
    {
        _putenv_s("OAI_API_TOKEN", envOpenAiToken);
    }
    else
    {
        _putenv_s("OAI_API_TOKEN", "");
    }
    if (envXAiToken != nullptr)
    {
        _putenv_s("XAI_API_TOKEN", envXAiToken);
    }
    else
    {
        _putenv_s("XAI_API_TOKEN", "");
    }
    if (envGoogleToken != nullptr)
    {
        _putenv_s("GOOGLE_API_TOKEN", envGoogleToken);
    }
    else
    {
        _putenv_s("GOOGLE_API_TOKEN", "");
    }
    if (envDefaultProvider != nullptr)
    {
        _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", envDefaultProvider);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", "");
    }
    if (envStatePath != nullptr)
    {
        _putenv_s("MYBUDDYBOT_STATE_PATH", envStatePath);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_STATE_PATH", "");
    }
    if (envBotName != nullptr)
    {
        _putenv_s("MYBUDDYBOT_NAME", envBotName);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_NAME", "");
    }
#else
    if (envDebugLevelMode != nullptr)
    {
        setenv("MYBUDDYBOT_DEBUG_LEVEL_MODE", envDebugLevelMode, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_DEBUG_LEVEL_MODE");
    }
    if (envDbPath != nullptr)
    {
        setenv("MYBUDDYBOT_DB_PATH", envDbPath, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_DB_PATH");
    }
    if (envBotToken != nullptr)
    {
        setenv("TG_API_TOKEN", envBotToken, 1);
    }
    else
    {
        unsetenv("TG_API_TOKEN");
    }
    if (envOpenAiToken != nullptr)
    {
        setenv("OAI_API_TOKEN", envOpenAiToken, 1);
    }
    else
    {
        unsetenv("OAI_API_TOKEN");
    }
    if (envXAiToken != nullptr)
    {
        setenv("XAI_API_TOKEN", envXAiToken, 1);
    }
    else
    {
        unsetenv("XAI_API_TOKEN");
    }
    if (envGoogleToken != nullptr)
    {
        setenv("GOOGLE_API_TOKEN", envGoogleToken, 1);
    }
    else
    {
        unsetenv("GOOGLE_API_TOKEN");
    }
    if (envDefaultProvider != nullptr)
    {
        setenv("MYBUDDYBOT_DEFAULT_PROVIDER", envDefaultProvider, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_DEFAULT_PROVIDER");
    }
    if (envStatePath != nullptr)
    {
        setenv("MYBUDDYBOT_STATE_PATH", envStatePath, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_STATE_PATH");
    }
    if (envBotName != nullptr)
    {
        setenv("MYBUDDYBOT_NAME", envBotName, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_NAME");
    }
#endif
}
} // namespace mbb::tests

#endif // APPCONFIGTEST_H
