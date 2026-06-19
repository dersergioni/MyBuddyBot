#ifndef ACCESSCONTROLCONFIGTEST_H
#define ACCESSCONTROLCONFIGTEST_H

#include "../core/Config.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace mbb::tests
{

namespace
{
std::optional<std::string> GetEnvValue(const char* name)
{
    const char* value = std::getenv(name);
    if (value != nullptr)
    {
        return std::string(value);
    }
    return std::nullopt;
}

void SetEnvValue(const char* name, const std::string& value)
{
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void UnsetEnvValue(const char* name)
{
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

void SetOptionalEnvValue(const char* name, const std::string& value)
{
    if (value.empty())
    {
        UnsetEnvValue(name);
        return;
    }
    SetEnvValue(name, value);
}
} // namespace

namespace detail
{
class EnvGuard
{
  public:
    explicit EnvGuard(std::initializer_list<const char*> names)
    {
        for (const char* name : names)
        {
            values_[name] = GetEnvValue(name);
        }
    }

    ~EnvGuard()
    {
        for (const auto& [name, value] : values_)
        {
            if (value.has_value())
            {
                SetEnvValue(name.c_str(), *value);
            }
            else
            {
                UnsetEnvValue(name.c_str());
            }
        }
    }

  private:
    std::unordered_map<std::string, std::optional<std::string>> values_;
};
} // namespace detail

class AccessControlConfigTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        envGuard_ = std::make_unique<detail::EnvGuard>(std::initializer_list<const char*>{
            "MYBUDDYBOT_DB_PATH",
            "TG_API_TOKEN",
            "OAI_API_TOKEN",
            "XAI_API_TOKEN",
            "GOOGLE_API_TOKEN",
            "MYBUDDYBOT_DEFAULT_PROVIDER",
            "MYBUDDYBOT_STATE_PATH",
            "MYBUDDYBOT_ALLOWLIST_IDS",
            "MYBUDDYBOT_ALLOWLIST_USERNAMES",
            "MYBUDDYBOT_BLOCKLIST_IDS",
            "MYBUDDYBOT_BLOCKLIST_USERNAMES",
            "MYBUDDYBOT_ADMIN_IDS",
            "MYBUDDYBOT_AI_CONFIG_PATH",
        });

        Config::SetTestMode(true);
        SetBaseConfigEnv();
        // These tests don't configure AI models, so providers stay disabled
        // (allowed in test mode); clear any leaked/ambient config path.
        UnsetEnvValue("MYBUDDYBOT_AI_CONFIG_PATH");
    }

    void TearDown() override
    {
        envGuard_.reset();
    }

    void InitWithAccessConfig(const std::string& allowIds,
                              const std::string& allowUsernames,
                              const std::string& blockIds,
                              const std::string& blockUsernames,
                              const std::string& adminIds)
    {
        SetOptionalEnvValue("MYBUDDYBOT_ALLOWLIST_IDS", allowIds);
        SetOptionalEnvValue("MYBUDDYBOT_ALLOWLIST_USERNAMES", allowUsernames);
        SetOptionalEnvValue("MYBUDDYBOT_BLOCKLIST_IDS", blockIds);
        SetOptionalEnvValue("MYBUDDYBOT_BLOCKLIST_USERNAMES", blockUsernames);
        SetOptionalEnvValue("MYBUDDYBOT_ADMIN_IDS", adminIds);
        Config::Init();
    }

  private:
    static void SetBaseConfigEnv()
    {
#if defined(_WIN32)
        SetEnvValue("MYBUDDYBOT_DB_PATH", "C:\\temp\\mybuddybot-access.db");
        SetEnvValue("MYBUDDYBOT_STATE_PATH", "C:\\temp\\mybuddybot-access.state");
#else
        SetEnvValue("MYBUDDYBOT_DB_PATH", "/tmp/mybuddybot-access.db");
        SetEnvValue("MYBUDDYBOT_STATE_PATH", "/tmp/mybuddybot-access.state");
#endif
        SetEnvValue("TG_API_TOKEN", "1234567890:ABC");
        SetEnvValue("OAI_API_TOKEN", "OPENAI123");
        UnsetEnvValue("XAI_API_TOKEN");
        UnsetEnvValue("GOOGLE_API_TOKEN");
        SetEnvValue("MYBUDDYBOT_DEFAULT_PROVIDER", "openai");

        UnsetEnvValue("MYBUDDYBOT_ALLOWLIST_IDS");
        UnsetEnvValue("MYBUDDYBOT_ALLOWLIST_USERNAMES");
        UnsetEnvValue("MYBUDDYBOT_BLOCKLIST_IDS");
        UnsetEnvValue("MYBUDDYBOT_BLOCKLIST_USERNAMES");
        UnsetEnvValue("MYBUDDYBOT_ADMIN_IDS");
    }

    std::unique_ptr<detail::EnvGuard> envGuard_;
};

TEST_F(AccessControlConfigTest, AdminBypassesBlocklist)
{
    InitWithAccessConfig("", "", "42", "@blocked_admin", "42");

    EXPECT_TRUE(Config::IsAdminUser(42));
    EXPECT_TRUE(Config::IsUserAuthorized(42, "@blocked_admin"));
}

TEST_F(AccessControlConfigTest, BlocklistIdDeniesAccess)
{
    InitWithAccessConfig("", "", "777", "", "");

    EXPECT_FALSE(Config::IsUserAuthorized(777, "regular_user"));
}

TEST_F(AccessControlConfigTest, BlocklistUsernameDeniesAccess)
{
    InitWithAccessConfig("", "", "", "@BaD_User", "");

    EXPECT_FALSE(Config::IsUserAuthorized(100, "bad_user"));
}

TEST_F(AccessControlConfigTest, EmptyAllowlistAllowsUser)
{
    InitWithAccessConfig("", "", "", "", "");

    EXPECT_TRUE(Config::IsUserAuthorized(101, "any_user"));
}

TEST_F(AccessControlConfigTest, NonEmptyAllowlistDeniesUnknownUser)
{
    InitWithAccessConfig("1", "", "", "", "");

    EXPECT_FALSE(Config::IsUserAuthorized(2, "outsider"));
}

TEST_F(AccessControlConfigTest, AllowlistIdAllowsUser)
{
    InitWithAccessConfig("55", "", "", "", "");

    EXPECT_TRUE(Config::IsUserAuthorized(55, ""));
}

TEST_F(AccessControlConfigTest, AllowlistUsernameAllowsUser)
{
    InitWithAccessConfig("", "@PoWeR_User", "", "", "");

    EXPECT_TRUE(Config::IsUserAuthorized(10, "power_user"));
}

TEST_F(AccessControlConfigTest, InvalidIdEntriesAreIgnored)
{
    InitWithAccessConfig("123,abc,456x,789", "", "", "", "");

    EXPECT_EQ(2u, Config::GetAllowlistIds().size());
    EXPECT_TRUE(Config::GetAllowlistIds().contains(123));
    EXPECT_TRUE(Config::GetAllowlistIds().contains(789));
    EXPECT_FALSE(Config::GetAllowlistIds().contains(456));
}

TEST_F(AccessControlConfigTest, EmptyUsernameStillUsesIdBranch)
{
    InitWithAccessConfig("55", "", "", "", "");
    EXPECT_TRUE(Config::IsUserAuthorized(55, ""));

    InitWithAccessConfig("", "", "55", "", "");
    EXPECT_FALSE(Config::IsUserAuthorized(55, ""));
}

} // namespace mbb::tests

#endif // ACCESSCONTROLCONFIGTEST_H
