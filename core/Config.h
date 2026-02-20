#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace mbb
{

enum class DialogMode
{
    None,
    ImageGeneration
};

enum class LogLevel
{
    Info,
    Debug,
    Error
};

enum class AiProvider
{
    OpenAI,
    XAI,
    Google
};

class Config
{
  public:
    Config() = delete;

    static void Init();

    [[nodiscard]] static bool IsDebugMode()
    {
        return isDebugMode_;
    }
    [[nodiscard]] static bool IsTestMode()
    {
        return isTestMode_;
    }
    [[nodiscard]] static const std::filesystem::path& GetDbPath()
    {
        return dbPath_;
    }
    [[nodiscard]] static const std::string& GetBotToken()
    {
        return botToken_;
    }
    [[nodiscard]] static const std::string& GetOpenAiToken()
    {
        return openAiToken_;
    }
    [[nodiscard]] static const std::string& GetXAiToken()
    {
        return xAiToken_;
    }
    [[nodiscard]] static const std::string& GetGoogleToken()
    {
        return googleToken_;
    }
    [[nodiscard]] static const std::string& GetBotName()
    {
        return botName_;
    }
    [[nodiscard]] static const std::filesystem::path& GetStatePath()
    {
        return statePath_;
    }
    [[nodiscard]] static AiProvider GetDefaultProvider()
    {
        return defaultProvider_;
    }
    [[nodiscard]] static const std::vector<AiProvider>& GetEnabledProviders()
    {
        return enabledProviders_;
    }
    [[nodiscard]] static bool IsProviderEnabled(AiProvider provider);
    [[nodiscard]] static const std::unordered_set<int64_t>& GetAllowlistIds()
    {
        return allowlistIds_;
    }
    [[nodiscard]] static const std::unordered_set<std::string>& GetAllowlistUsernames()
    {
        return allowlistUsernames_;
    }
    [[nodiscard]] static const std::unordered_set<int64_t>& GetBlocklistIds()
    {
        return blocklistIds_;
    }
    [[nodiscard]] static const std::unordered_set<std::string>& GetBlocklistUsernames()
    {
        return blocklistUsernames_;
    }
    [[nodiscard]] static const std::unordered_set<int64_t>& GetAdminIds()
    {
        return adminIds_;
    }
    [[nodiscard]] static bool IsAdminUser(int64_t userId);
    [[nodiscard]] static bool IsUserAuthorized(int64_t userId, const std::string& username);

    // For testing
    static void SetTestMode(bool value)
    {
        isTestMode_ = value;
    }

  private:
    static bool isDebugMode_;
    static bool isTestMode_;
    static std::filesystem::path dbPath_;
    static std::filesystem::path statePath_;
    static std::string botToken_;
    static std::string openAiToken_;
    static std::string xAiToken_;
    static std::string googleToken_;
    static std::string botName_;
    static AiProvider defaultProvider_;
    static std::vector<AiProvider> enabledProviders_;
    static std::unordered_set<int64_t> allowlistIds_;
    static std::unordered_set<std::string> allowlistUsernames_;
    static std::unordered_set<int64_t> blocklistIds_;
    static std::unordered_set<std::string> blocklistUsernames_;
    static std::unordered_set<int64_t> adminIds_;
};

} // namespace mbb
