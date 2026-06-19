#include "bot/UserState.h"

#include "core/Logger.h"

#include <cstring>
#include <fstream>

namespace mbb
{

namespace
{
constexpr uint32_t kStateFileVersion = 3;
} // namespace

void UserState::SetDialogMode(const ChatKey& key, DialogMode mode)
{
    std::lock_guard lock(mutex_);
    dialogModes_[key] = mode;
}

DialogMode UserState::GetDialogMode(const ChatKey& key) const
{
    std::lock_guard lock(mutex_);
    auto it = dialogModes_.find(key);
    return (it != dialogModes_.end()) ? it->second : DialogMode::None;
}

void UserState::SetAudioResponse(const ChatKey& key, bool enabled)
{
    std::lock_guard lock(mutex_);
    audioResponses_[key] = enabled;
    Save();
}

bool UserState::GetAudioResponse(const ChatKey& key) const
{
    std::lock_guard lock(mutex_);
    auto it = audioResponses_.find(key);
    return (it != audioResponses_.end()) ? it->second : false;
}

void UserState::ToggleAudioResponse(const ChatKey& key)
{
    std::lock_guard lock(mutex_);
    audioResponses_[key] = !audioResponses_[key];
    Save();
}

void UserState::SetAiProvider(const ChatKey& key, AiProvider provider)
{
    std::lock_guard lock(mutex_);
    aiProviders_[key] = provider;
    Save();
}

AiProvider UserState::GetAiProvider(const ChatKey& key) const
{
    std::lock_guard lock(mutex_);
    auto it = aiProviders_.find(key);
    return (it != aiProviders_.end()) ? it->second : Config::GetDefaultProvider();
}

void UserState::ToggleAiProvider(const ChatKey& key)
{
    std::lock_guard lock(mutex_);
    auto& provider = aiProviders_[key];
    switch (provider)
    {
    case AiProvider::OpenAI:
        provider = AiProvider::XAI;
        break;
    case AiProvider::XAI:
        provider = AiProvider::Google;
        break;
    case AiProvider::Google:
        provider = AiProvider::OpenAI;
        break;
    }
    Save();
}

void UserState::SetModelSelector(const ChatKey& key, ModelSelector selector)
{
    std::lock_guard lock(mutex_);
    modelSelectors_[key] = selector;
    Save();
}

ModelSelector UserState::GetModelSelector(const ChatKey& key) const
{
    std::lock_guard lock(mutex_);
    auto it = modelSelectors_.find(key);
    return (it != modelSelectors_.end()) ? it->second : ModelSelector::Primary;
}

void UserState::ToggleModelSelector(const ChatKey& key)
{
    std::lock_guard lock(mutex_);
    auto& selector = modelSelectors_[key];
    selector = (selector == ModelSelector::Primary) ? ModelSelector::Secondary : ModelSelector::Primary;
    Save();
}

void UserState::Clear(const ChatKey& key)
{
    std::lock_guard lock(mutex_);
    dialogModes_.erase(key);
    audioResponses_.erase(key);
    pendingImages_.erase(key);
    lastProcessedMediaGroupId_.erase(key);
    // Keep AI provider setting even after clear
}

void UserState::AddPendingImage(const ChatKey& key, std::string imageBase64)
{
    std::lock_guard lock(mutex_);
    pendingImages_[key].push_back(std::move(imageBase64));
}

std::vector<std::string> UserState::TakePendingImages(const ChatKey& key)
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> result;
    auto it = pendingImages_.find(key);
    if (it != pendingImages_.end())
    {
        result = std::move(it->second);
        pendingImages_.erase(it);
    }
    return result;
}

void UserState::ClearPendingImages(const ChatKey& key)
{
    std::lock_guard lock(mutex_);
    pendingImages_.erase(key);
}

void UserState::SetLastProcessedMediaGroupId(const ChatKey& key, const std::string& mediaGroupId)
{
    std::lock_guard lock(mutex_);
    lastProcessedMediaGroupId_[key] = mediaGroupId;
}

bool UserState::IsFromLastProcessedMediaGroup(const ChatKey& key, const std::string& mediaGroupId) const
{
    std::lock_guard lock(mutex_);
    auto it = lastProcessedMediaGroupId_.find(key);
    return it != lastProcessedMediaGroupId_.end() && it->second == mediaGroupId;
}

void UserState::SetActiveModule(const ActiveModuleKey& key, const std::string& modulePrefix)
{
    std::lock_guard lock(mutex_);
    activeModules_[key] = modulePrefix;
    Save();
}

std::string UserState::GetActiveModule(const ActiveModuleKey& key) const
{
    std::lock_guard lock(mutex_);
    auto it = activeModules_.find(key);
    return (it != activeModules_.end()) ? it->second : std::string{};
}

void UserState::ClearActiveModule(const ActiveModuleKey& key)
{
    std::lock_guard lock(mutex_);
    activeModules_.erase(key);
    Save();
}

void UserState::Save() const
{
    if (!statePath_.empty())
        WriteState(statePath_);
}

void UserState::SaveToFile(const std::filesystem::path& path) const
{
    std::lock_guard lock(mutex_);
    WriteState(path);
}

void UserState::WriteState(const std::filesystem::path& path) const
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        Logger::Error("Failed to open state file for writing: " + path.string());
        return;
    }

    // Header
    file.write("MBB\0", 4);
    file.write(reinterpret_cast<const char*>(&kStateFileVersion), 4);

    // Section 1: aiProviders
    uint32_t aiProvidersCount = static_cast<uint32_t>(aiProviders_.size());
    file.write(reinterpret_cast<const char*>(&aiProvidersCount), 4);
    for (const auto& [key, provider] : aiProviders_)
    {
        file.write(reinterpret_cast<const char*>(&key.first), 8);
        file.write(reinterpret_cast<const char*>(&key.second), 4);
        uint8_t providerByte = static_cast<uint8_t>(provider);
        file.write(reinterpret_cast<const char*>(&providerByte), 1);
    }

    // Section 2: audioResponses
    uint32_t audioResponsesCount = static_cast<uint32_t>(audioResponses_.size());
    file.write(reinterpret_cast<const char*>(&audioResponsesCount), 4);
    for (const auto& [key, enabled] : audioResponses_)
    {
        file.write(reinterpret_cast<const char*>(&key.first), 8);
        file.write(reinterpret_cast<const char*>(&key.second), 4);
        uint8_t enabledByte = enabled ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&enabledByte), 1);
    }

    // Section 3: modelSelectors
    uint32_t modelSelectorsCount = static_cast<uint32_t>(modelSelectors_.size());
    file.write(reinterpret_cast<const char*>(&modelSelectorsCount), 4);
    for (const auto& [key, selector] : modelSelectors_)
    {
        file.write(reinterpret_cast<const char*>(&key.first), 8);
        file.write(reinterpret_cast<const char*>(&key.second), 4);
        uint8_t selectorByte = static_cast<uint8_t>(selector);
        file.write(reinterpret_cast<const char*>(&selectorByte), 1);
    }

    // Section 4: activeModules
    uint32_t activeModulesCount = static_cast<uint32_t>(activeModules_.size());
    file.write(reinterpret_cast<const char*>(&activeModulesCount), 4);
    for (const auto& [key, prefix] : activeModules_)
    {
        file.write(reinterpret_cast<const char*>(&key.chatId), 8);
        file.write(reinterpret_cast<const char*>(&key.threadId), 4);
        file.write(reinterpret_cast<const char*>(&key.userId), 8);
        uint32_t len = static_cast<uint32_t>(prefix.size());
        file.write(reinterpret_cast<const char*>(&len), 4);
        file.write(prefix.data(), len);
    }

    Logger::Debug("Saved state: " + std::to_string(aiProvidersCount) + " providers, " +
                  std::to_string(audioResponsesCount) + " audio settings, " + std::to_string(modelSelectorsCount) +
                  " model selectors, " + std::to_string(activeModulesCount) + " active modules");
}

void UserState::LoadFromFile(const std::filesystem::path& path)
{
    std::lock_guard lock(mutex_);

    statePath_ = path;

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        Logger::Info("No state file found, starting fresh");
        return;
    }

    // Read and validate header
    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "MBB\0", 4) != 0)
    {
        Logger::Info("Invalid state file magic, starting fresh");
        return;
    }

    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), 4);

    if (version < 1 || version > kStateFileVersion)
    {
        Logger::Info("Unknown state file version: " + std::to_string(version));
        return;
    }

    // Section 1: aiProviders
    uint32_t aiProvidersCount;
    file.read(reinterpret_cast<char*>(&aiProvidersCount), 4);
    for (uint32_t i = 0; i < aiProvidersCount; ++i)
    {
        int64_t chatId;
        int32_t threadId;
        uint8_t provider;

        file.read(reinterpret_cast<char*>(&chatId), 8);
        file.read(reinterpret_cast<char*>(&threadId), 4);
        file.read(reinterpret_cast<char*>(&provider), 1);

        if (!file)
        {
            Logger::Error("Error reading aiProviders at entry " + std::to_string(i));
            return;
        }

        aiProviders_[{chatId, threadId}] = static_cast<AiProvider>(provider);
    }

    // Section 2: audioResponses
    uint32_t audioResponsesCount;
    file.read(reinterpret_cast<char*>(&audioResponsesCount), 4);
    for (uint32_t i = 0; i < audioResponsesCount; ++i)
    {
        int64_t chatId;
        int32_t threadId;
        uint8_t enabled;

        file.read(reinterpret_cast<char*>(&chatId), 8);
        file.read(reinterpret_cast<char*>(&threadId), 4);
        file.read(reinterpret_cast<char*>(&enabled), 1);

        if (!file)
        {
            Logger::Error("Error reading audioResponses at entry " + std::to_string(i));
            return;
        }

        audioResponses_[{chatId, threadId}] = (enabled != 0);
    }

    // Section 3: modelSelectors (optional — old files may not have this)
    uint32_t modelSelectorsCount = 0;
    file.read(reinterpret_cast<char*>(&modelSelectorsCount), 4);
    if (file)
    {
        for (uint32_t i = 0; i < modelSelectorsCount; ++i)
        {
            int64_t chatId;
            int32_t threadId;
            uint8_t selector;

            file.read(reinterpret_cast<char*>(&chatId), 8);
            file.read(reinterpret_cast<char*>(&threadId), 4);
            file.read(reinterpret_cast<char*>(&selector), 1);

            if (!file)
            {
                Logger::Error("Error reading modelSelectors at entry " + std::to_string(i));
                return;
            }

            modelSelectors_[{chatId, threadId}] = static_cast<ModelSelector>(selector);
        }
    }

    // Section 4: activeModules (added in v2, user-scoped in v3)
    uint32_t activeModulesCount = 0;
    if (version >= 2)
    {
        file.read(reinterpret_cast<char*>(&activeModulesCount), 4);
        for (uint32_t i = 0; i < activeModulesCount; ++i)
        {
            int64_t chatId;
            int32_t threadId;
            int64_t userId = 0;
            uint32_t len;

            file.read(reinterpret_cast<char*>(&chatId), 8);
            file.read(reinterpret_cast<char*>(&threadId), 4);
            if (version >= 3)
            {
                file.read(reinterpret_cast<char*>(&userId), 8);
            }
            file.read(reinterpret_cast<char*>(&len), 4);

            if (!file || len > 256)
            {
                Logger::Error("Error reading activeModules at entry " + std::to_string(i));
                return;
            }

            std::string prefix(len, '\0');
            file.read(prefix.data(), len);

            if (!file)
            {
                Logger::Error("Error reading activeModules prefix at entry " + std::to_string(i));
                return;
            }

            if (version >= 3)
            {
                activeModules_[{chatId, threadId, userId}] = std::move(prefix);
            }
        }
    }

    Logger::Info("Loaded state: " + std::to_string(aiProvidersCount) + " providers, " +
                 std::to_string(audioResponsesCount) + " audio settings, " + std::to_string(modelSelectorsCount) +
                 " model selectors, " + std::to_string(activeModulesCount) + " active modules");
}

} // namespace mbb
