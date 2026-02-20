#pragma once

#include "bot/UserState.h"
#include "core/Storage.h"

#include <tgbot/tgbot.h>

#include <memory>
#include <string>

namespace mbb
{

class TelegramApi;
class MediaDownloader;
class TaskQueue;
class IAiService;

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
                    std::shared_ptr<IAiService> google);

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

    // -------------------------------------------------------------------------
    // Message handlers
    // -------------------------------------------------------------------------
    void HandleAnyMessage(const TgBot::Message::Ptr& message);
    void HandleNonCommandMessage(const TgBot::Message::Ptr& message);

  private:
    std::shared_ptr<TelegramApi> api_;
    std::shared_ptr<MediaDownloader> downloader_;
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<UserState> state_;
    std::shared_ptr<TaskQueue> taskQueue_;
    std::shared_ptr<IAiService> openAi_;
    std::shared_ptr<IAiService> xAi_;
    std::shared_ptr<IAiService> google_;

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
