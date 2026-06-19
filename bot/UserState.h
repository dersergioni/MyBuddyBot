#pragma once

#include "ai/IAiService.h"
#include "core/Config.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mbb
{

// =============================================================================
// Types
// =============================================================================

using ChatKey = std::pair<int64_t, int32_t>; // {chatId, threadId}

struct ActiveModuleKey
{
    int64_t chatId;
    int32_t threadId;
    int64_t userId;

    bool operator==(const ActiveModuleKey& other) const noexcept
    {
        return chatId == other.chatId && threadId == other.threadId && userId == other.userId;
    }
};

// =============================================================================
// UserState - Thread-safe state management for chat sessions
// =============================================================================

class UserState
{
  public:
    UserState() = default;
    ~UserState() = default;

    // Non-copyable
    UserState(const UserState&) = delete;
    UserState& operator=(const UserState&) = delete;

    // Dialog mode
    void SetDialogMode(const ChatKey& key, DialogMode mode);
    [[nodiscard]] DialogMode GetDialogMode(const ChatKey& key) const;

    // Audio response setting
    void SetAudioResponse(const ChatKey& key, bool enabled);
    [[nodiscard]] bool GetAudioResponse(const ChatKey& key) const;
    void ToggleAudioResponse(const ChatKey& key);

    // AI provider selection
    void SetAiProvider(const ChatKey& key, AiProvider provider);
    [[nodiscard]] AiProvider GetAiProvider(const ChatKey& key) const;
    void ToggleAiProvider(const ChatKey& key);

    // Model selector (primary/secondary)
    void SetModelSelector(const ChatKey& key, ModelSelector selector);
    [[nodiscard]] ModelSelector GetModelSelector(const ChatKey& key) const;
    void ToggleModelSelector(const ChatKey& key);

    // Clear all state for a chat
    void Clear(const ChatKey& key);

    // Persistence
    void SaveToFile(const std::filesystem::path& path) const;
    void LoadFromFile(const std::filesystem::path& path);

    // Pending reference images for image generation
    void AddPendingImage(const ChatKey& key, std::string imageBase64);
    [[nodiscard]] std::vector<std::string> TakePendingImages(const ChatKey& key);
    void ClearPendingImages(const ChatKey& key);

    // Media group tracking (to ignore duplicate photos from same group)
    void SetLastProcessedMediaGroupId(const ChatKey& key, const std::string& mediaGroupId);
    [[nodiscard]] bool IsFromLastProcessedMediaGroup(const ChatKey& key, const std::string& mediaGroupId) const;

    // Active module tracking
    void SetActiveModule(const ActiveModuleKey& key, const std::string& modulePrefix);
    [[nodiscard]] std::string GetActiveModule(const ActiveModuleKey& key) const;
    void ClearActiveModule(const ActiveModuleKey& key);

  private:
    void Save() const;
    void WriteState(const std::filesystem::path& path) const;

    struct PairHash
    {
        std::size_t operator()(const ChatKey& p) const noexcept
        {
            std::size_t h1 = std::hash<int64_t>{}(p.first);
            std::size_t h2 = std::hash<int32_t>{}(p.second);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    struct ActiveModuleKeyHash
    {
        std::size_t operator()(const ActiveModuleKey& key) const noexcept
        {
            std::size_t h1 = std::hash<int64_t>{}(key.chatId);
            std::size_t h2 = std::hash<int32_t>{}(key.threadId);
            std::size_t h3 = std::hash<int64_t>{}(key.userId);
            std::size_t hash = h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            return hash ^ (h3 + 0x9e3779b9 + (hash << 6) + (hash >> 2));
        }
    };

    mutable std::mutex mutex_;
    std::filesystem::path statePath_;
    std::unordered_map<ChatKey, DialogMode, PairHash> dialogModes_;
    std::unordered_map<ChatKey, bool, PairHash> audioResponses_;
    std::unordered_map<ChatKey, AiProvider, PairHash> aiProviders_;
    std::unordered_map<ChatKey, ModelSelector, PairHash> modelSelectors_;
    std::unordered_map<ChatKey, std::vector<std::string>, PairHash> pendingImages_;
    std::unordered_map<ChatKey, std::string, PairHash> lastProcessedMediaGroupId_;
    std::unordered_map<ActiveModuleKey, std::string, ActiveModuleKeyHash> activeModules_;
};

} // namespace mbb
