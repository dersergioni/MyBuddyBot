#pragma once

#include "bot/UserState.h"
#include "core/Storage.h"

#include <tgbot/tgbot.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mbb
{

class TelegramApi;
class MediaDownloader;
class TaskQueue;
class IAiService;
class ModuleHost;

// =============================================================================
// CommandHandlers - Handles all bot commands and message processing
// =============================================================================

class CommandHandlers
{
  public:
    CommandHandlers(std::shared_ptr<TelegramApi> api,
                    std::shared_ptr<MediaDownloader> downloader,
                    std::shared_ptr<Storage> storage,
                    std::shared_ptr<UserState> state,
                    std::shared_ptr<TaskQueue> taskQueue,
                    std::shared_ptr<IAiService> openAi,
                    std::shared_ptr<IAiService> xAi,
                    std::shared_ptr<IAiService> google,
                    std::shared_ptr<ModuleHost> moduleHost);

    ~CommandHandlers() = default;

    // Non-copyable
    CommandHandlers(const CommandHandlers&) = delete;
    CommandHandlers& operator=(const CommandHandlers&) = delete;

    // -------------------------------------------------------------------------
    // Command handlers
    // -------------------------------------------------------------------------
    void HandleStart(const TgBot::Message::Ptr& message);
    void HandleClear(const TgBot::Message::Ptr& message);
    void HandleSwitchProvider(const TgBot::Message::Ptr& message);
    void HandleSwitchModel(const TgBot::Message::Ptr& message);
    void HandleAudio(const TgBot::Message::Ptr& message);
    void HandleImage(const TgBot::Message::Ptr& message);
    void HandleSystem(const TgBot::Message::Ptr& message);
    void HandleHealth(const TgBot::Message::Ptr& message);
    void HandleBroadcast(const TgBot::Message::Ptr& message);

    // -------------------------------------------------------------------------
    // Message handlers
    // -------------------------------------------------------------------------
    void HandleAnyMessage(const TgBot::Message::Ptr& message);
    void HandleNonCommandMessage(const TgBot::Message::Ptr& message);

    // -------------------------------------------------------------------------
    // Callback query handler
    // -------------------------------------------------------------------------
    void HandleCallbackQuery(const TgBot::CallbackQuery::Ptr& query);

  private:
    struct PendingModuleActivation
    {
        TgBot::Message::Ptr message;
        std::string moduleCallbackPrefix;
    };

    struct PendingBroadcast
    {
        std::string text;
        int32_t token = 0;
    };

    std::shared_ptr<TelegramApi> api_;
    std::shared_ptr<MediaDownloader> downloader_;
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<UserState> state_;
    std::shared_ptr<TaskQueue> taskQueue_;
    std::shared_ptr<IAiService> openAi_;
    std::shared_ptr<IAiService> xAi_;
    std::shared_ptr<IAiService> google_;
    std::shared_ptr<ModuleHost> moduleHost_;
    mutable std::mutex pendingActivationMutex_;
    std::unordered_map<int64_t, PendingModuleActivation> pendingActivations_;
    mutable std::mutex pendingBroadcastMutex_;
    std::unordered_map<int64_t, PendingBroadcast> pendingBroadcasts_;
    std::atomic<int32_t> broadcastTokenSeq_{0};

    // -------------------------------------------------------------------------
    // Message & callback routing
    // -------------------------------------------------------------------------
    [[nodiscard]] bool TryRouteMessageToModule(const TgBot::Message::Ptr& message);
    [[nodiscard]] bool TryRouteCallbackToModule(const TgBot::CallbackQuery::Ptr& query);
    void ContinueStandardNonCommandFlow(const TgBot::Message::Ptr& message);
    void SendTriggerConfirmation(const TgBot::Message::Ptr& message,
                                 const std::string& triggerKeyword,
                                 const std::string& moduleDisplayName,
                                 const std::string& moduleCallbackPrefix);
    [[nodiscard]] bool HandleTriggerConfirmationCallback(const TgBot::CallbackQuery::Ptr& query);
    [[nodiscard]] bool HandleBroadcastConfirmationCallback(const TgBot::CallbackQuery::Ptr& query);

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------
    [[nodiscard]] bool IsAuthorizedMessageUser(const TgBot::Message::Ptr& message) const;
    [[nodiscard]] bool IsAdminMessageUser(const TgBot::Message::Ptr& message) const;
    void ReplyUnauthorized(const TgBot::Message::Ptr& message) const;
    void LogCommandInvocation(const TgBot::Message::Ptr& message, const std::string& command) const;
    [[nodiscard]] std::shared_ptr<IAiService> GetCurrentAi(const ChatKey& key) const;
    [[nodiscard]] Storage::Role GetAssistantRole(const ChatKey& key) const;

    // -------------------------------------------------------------------------
    // Async message processing
    // -------------------------------------------------------------------------
    void ProcessTextAsync(const TgBot::Message::Ptr& message, const std::shared_ptr<IAiService>& ai);

    void ProcessVoiceAsync(const TgBot::Message::Ptr& message, const std::shared_ptr<IAiService>& ai);

    void ProcessImageAsync(const TgBot::Message::Ptr& message, const std::shared_ptr<IAiService>& ai);
};

} // namespace mbb
