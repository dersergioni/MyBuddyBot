#include "core/Config.h"

#include "core/Logger.h"
#include "infra/Env.h"
#include "infra/StringUtils.h"

#include <fmt/format.h>
#include <fmt/std.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace mbb
{

bool Config::isDebugMode_ = false;
bool Config::isTestMode_ = false;
std::filesystem::path Config::dbPath_;
std::filesystem::path Config::statePath_;
std::string Config::botToken_;
std::string Config::openAiToken_;
std::string Config::xAiToken_;
std::string Config::googleToken_;
std::string Config::botName_;
AiProvider Config::defaultProvider_ = AiProvider::OpenAI;
std::vector<AiProvider> Config::enabledProviders_;
std::unordered_set<int64_t> Config::allowlistIds_;
std::unordered_set<std::string> Config::allowlistUsernames_;
std::unordered_set<int64_t> Config::blocklistIds_;
std::unordered_set<std::string> Config::blocklistUsernames_;
std::unordered_set<int64_t> Config::adminIds_;
std::string Config::viewerUrl_;
std::filesystem::path Config::viewerDir_;
AiConfig Config::aiConfig_;

namespace
{
const char* ProviderConfigName(AiProvider provider)
{
    switch (provider)
    {
    case AiProvider::OpenAI:
        return "openai";
    case AiProvider::XAI:
        return "xai";
    case AiProvider::Google:
        return "google";
    }
    return "";
}

std::string ResolveProviderKey(const AiConfig& aiConfig, const char* envVar, const char* provider)
{
    std::string key = StringUtils::TrimCopy(Env::GetOptional(envVar));
    if (key.empty())
    {
        key = StringUtils::TrimCopy(aiConfig.GetApiKey(provider).value_or(""));
    }
    return key;
}

AiProvider ParseProvider(const std::string& value)
{
    const std::string normalized = StringUtils::ToLower(value);

    if (normalized == "openai")
    {
        return AiProvider::OpenAI;
    }
    if (normalized == "xai")
    {
        return AiProvider::XAI;
    }
    if (normalized == "google")
    {
        return AiProvider::Google;
    }
    throw std::runtime_error(fmt::format("Invalid provider value: {}", value));
}

std::vector<std::string> SplitCsv(const std::string& value)
{
    std::vector<std::string> tokens;
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        token = StringUtils::TrimCopy(token);
        if (!token.empty())
        {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::unordered_set<int64_t> ParseIdSet(const std::string& csv, const char* envName)
{
    std::unordered_set<int64_t> result;
    for (const auto& token : SplitCsv(csv))
    {
        try
        {
            size_t pos = 0;
            long long parsed = std::stoll(token, &pos, 10);
            if (pos != token.size())
            {
                throw std::invalid_argument("Trailing characters");
            }
            result.insert(static_cast<int64_t>(parsed));
        }
        catch (const std::exception&)
        {
            Logger::Error(fmt::format("Invalid ID '{}' in {}. Ignoring entry.", token, envName));
        }
    }
    return result;
}

std::unordered_set<std::string> ParseUsernameSet(const std::string& csv)
{
    std::unordered_set<std::string> result;
    for (const auto& token : SplitCsv(csv))
    {
        std::string normalized = StringUtils::NormalizeUsername(token);
        if (!normalized.empty())
        {
            result.insert(std::move(normalized));
        }
    }
    return result;
}

std::filesystem::path GetExecutableDir()
{
    std::filesystem::path exePath;

#if defined(_WIN32)
    constexpr DWORD maxPathLength = 32768;
    for (DWORD bufferSize = MAX_PATH; bufferSize <= maxPathLength; bufferSize *= 2)
    {
        std::vector<wchar_t> buffer(bufferSize);
        DWORD len = GetModuleFileNameW(nullptr, buffer.data(), bufferSize);

        bool failed = (len == 0);
        bool success = (len > 0 && len < bufferSize);

        if (failed)
            break;
        if (success)
        {
            exePath = buffer.data();
            break;
        }
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0)
    {
        exePath = buffer.data();
    }
#elif defined(__linux__)
    std::error_code ec;
    exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
#endif

    if (exePath.empty())
    {
        Logger::Error("Failed to get executable path, using current directory");
        return std::filesystem::current_path();
    }

    return exePath.parent_path();
}

std::filesystem::path ToAbsolutePath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return path;
    }

    std::error_code ec;
    auto absolutePath = std::filesystem::absolute(path, ec);
    if (ec)
    {
        Logger::Error(fmt::format("Failed to resolve absolute path '{}': {}", path, ec.message()));
        return path;
    }

    return absolutePath.lexically_normal();
}
} // anonymous namespace

void Config::Init()
{
    // Debug mode (optional)
    if (Env::Has("MYBUDDYBOT_DEBUG_LEVEL_MODE"))
    {
        isDebugMode_ = true;
        Logger::Info("Debug level mode enabled");
    }

    botName_ = StringUtils::TrimCopy(Env::GetOptional("MYBUDDYBOT_NAME"));
    if (botName_.empty())
    {
        botName_ = "MyBuddyBot";
    }
    Logger::Info(fmt::format("Bot name: {}", botName_));

    dbPath_ = ToAbsolutePath(Env::GetRequired("MYBUDDYBOT_DB_PATH"));
    Logger::Info(fmt::format("DB path: {}", dbPath_));

    // State file path: optional override or same directory as executable
    const auto customStatePath = Env::GetOptional("MYBUDDYBOT_STATE_PATH");
    if (!customStatePath.empty())
    {
        statePath_ = ToAbsolutePath(customStatePath);
    }
    else
    {
        statePath_ = ToAbsolutePath(GetExecutableDir() / "MyBuddyBotState.bin");
    }
    Logger::Info(fmt::format("State path: {}", statePath_));

    botToken_ = Env::GetRequired("TG_API_TOKEN");

    aiConfig_ = AiConfig{};
    const auto aiConfigPath = StringUtils::TrimCopy(Env::GetOptional("MYBUDDYBOT_AI_CONFIG_PATH"));
    if (!aiConfigPath.empty())
    {
        aiConfig_.LoadFromFile(ToAbsolutePath(aiConfigPath));
    }

    openAiToken_ = ResolveProviderKey(aiConfig_, "OAI_API_TOKEN", "openai");
    xAiToken_ = ResolveProviderKey(aiConfig_, "XAI_API_TOKEN", "xai");
    googleToken_ = ResolveProviderKey(aiConfig_, "GOOGLE_API_TOKEN", "google");

    allowlistIds_ = ParseIdSet(Env::GetOptional("MYBUDDYBOT_ALLOWLIST_IDS"), "MYBUDDYBOT_ALLOWLIST_IDS");
    allowlistUsernames_ = ParseUsernameSet(Env::GetOptional("MYBUDDYBOT_ALLOWLIST_USERNAMES"));
    blocklistIds_ = ParseIdSet(Env::GetOptional("MYBUDDYBOT_BLOCKLIST_IDS"), "MYBUDDYBOT_BLOCKLIST_IDS");
    blocklistUsernames_ = ParseUsernameSet(Env::GetOptional("MYBUDDYBOT_BLOCKLIST_USERNAMES"));
    adminIds_ = ParseIdSet(Env::GetOptional("MYBUDDYBOT_ADMIN_IDS"), "MYBUDDYBOT_ADMIN_IDS");

    Logger::Info(fmt::format("Access control lists loaded: allow_ids={}, allow_usernames={}, block_ids={}, "
                             "block_usernames={}, admin_ids={}",
                             allowlistIds_.size(), allowlistUsernames_.size(), blocklistIds_.size(),
                             blocklistUsernames_.size(), adminIds_.size()));

    viewerUrl_ = StringUtils::TrimCopy(Env::GetOptional("MYBUDDYBOT_VIEWER_URL"));
    if (!viewerUrl_.empty())
    {
        Logger::Info(fmt::format("Viewer URL: {}", viewerUrl_));
    }

    const auto viewerDirEnv = Env::GetOptional("MYBUDDYBOT_VIEWER_DIR");
    if (!viewerDirEnv.empty())
    {
        viewerDir_ = ToAbsolutePath(viewerDirEnv);
        Logger::Info(fmt::format("Viewer dir: {}", viewerDir_));
    }

    if (!viewerUrl_.empty() && !viewerDir_.empty())
    {
        Logger::Info("Viewer feature: ENABLED");
    }
    else if (!viewerUrl_.empty() || !viewerDir_.empty())
    {
        Logger::Error("Viewer feature: DISABLED (both MYBUDDYBOT_VIEWER_URL and MYBUDDYBOT_VIEWER_DIR must be set)");
    }

    enabledProviders_.clear();
    if (!openAiToken_.empty() && aiConfig_.GetModelSpec("openai", "primary").IsComplete())
    {
        enabledProviders_.push_back(AiProvider::OpenAI);
    }
    if (!xAiToken_.empty() && aiConfig_.GetModelSpec("xai", "primary").IsComplete())
    {
        enabledProviders_.push_back(AiProvider::XAI);
    }
    if (!googleToken_.empty() && aiConfig_.GetModelSpec("google", "primary").IsComplete())
    {
        enabledProviders_.push_back(AiProvider::Google);
    }

    if (enabledProviders_.empty() && !isTestMode_)
    {
        throw std::runtime_error("No usable AI provider configured. Each provider needs an API key (env or config "
                                 "file) and a complete \"primary\" model in the AI config file "
                                 "(set MYBUDDYBOT_AI_CONFIG_PATH).");
    }

    // Default provider (optional)
    defaultProvider_ = AiProvider::OpenAI;
    const auto defaultProviderEnv = Env::GetOptional("MYBUDDYBOT_DEFAULT_PROVIDER");
    if (!defaultProviderEnv.empty())
    {
        try
        {
            defaultProvider_ = ParseProvider(defaultProviderEnv);
        }
        catch (const std::exception& e)
        {
            Logger::Error(fmt::format("Invalid MYBUDDYBOT_DEFAULT_PROVIDER: {}", e.what()));
            defaultProvider_ = AiProvider::OpenAI;
        }
    }

    if (!IsProviderEnabled(defaultProvider_) && !enabledProviders_.empty())
    {
        defaultProvider_ = enabledProviders_.front();
        Logger::Info(fmt::format("Default provider is not enabled. Falling back to {}",
                                 defaultProvider_ == AiProvider::OpenAI ? "OpenAI"
                                 : defaultProvider_ == AiProvider::XAI  ? "xAI"
                                                                        : "Google"));
    }
}

bool Config::IsProviderEnabled(AiProvider provider)
{
    return std::find(enabledProviders_.begin(), enabledProviders_.end(), provider) != enabledProviders_.end();
}

AiModelSpec Config::GetModelSpec(AiProvider provider, const std::string& slot)
{
    return aiConfig_.GetModelSpec(ProviderConfigName(provider), slot);
}

bool Config::IsAdminUser(int64_t userId)
{
    return adminIds_.contains(userId);
}

bool Config::IsUserAuthorized(int64_t userId, const std::string& username)
{
    const std::string normalizedUsername = StringUtils::NormalizeUsername(username);

    if (IsAdminUser(userId))
    {
        return true;
    }
    if (blocklistIds_.contains(userId))
    {
        return false;
    }
    if (!normalizedUsername.empty() && blocklistUsernames_.contains(normalizedUsername))
    {
        return false;
    }

    const bool allowlistConfigured = !allowlistIds_.empty() || !allowlistUsernames_.empty();
    if (!allowlistConfigured)
    {
        return true;
    }
    if (allowlistIds_.contains(userId))
    {
        return true;
    }
    if (!normalizedUsername.empty() && allowlistUsernames_.contains(normalizedUsername))
    {
        return true;
    }
    return false;
}

} // namespace mbb
