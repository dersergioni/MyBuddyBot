#pragma once

#include <atomic>
#include <memory>

namespace TgBot
{
class Bot;
class CurlHttpClient;
} // namespace TgBot

namespace mbb
{

class Storage;
class HttpClient;
class TaskQueue;
class TelegramApi;
class MediaDownloader;
class UserState;
class IAiService;
class IMessageWorker;
class CommandHandlers;
class ModuleHost;

namespace tests
{
class IntegrationTest;
}

// =============================================================================
// BotApp - Main application class
// =============================================================================

class BotApp
{
    friend class tests::IntegrationTest;

  public:
    BotApp();
    ~BotApp();

    // Non-copyable
    BotApp(const BotApp&) = delete;
    BotApp& operator=(const BotApp&) = delete;

    // Run the bot (blocking)
    int Run();

    // Stop the bot gracefully
    void Stop();

  private:
    void InitializeComponents();
    void RegisterHandlers();
    void SetBotCommands();

    std::atomic<bool> running_{true};

    // Components (order matters for destruction)
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<HttpClient> http_;
    std::shared_ptr<TaskQueue> taskQueue_;
    std::unique_ptr<TgBot::CurlHttpClient> tgBotHttp_;
    std::shared_ptr<TgBot::Bot> bot_;
    std::shared_ptr<TelegramApi> telegramApi_;
    std::shared_ptr<MediaDownloader> mediaDownloader_;
    std::shared_ptr<UserState> userState_;
    std::shared_ptr<IMessageWorker> messageWorker_;
    std::shared_ptr<IAiService> openAiService_;
    std::shared_ptr<IAiService> xAiService_;
    std::shared_ptr<IAiService> googleService_;
    std::shared_ptr<CommandHandlers> handlers_;
    std::shared_ptr<ModuleHost> moduleHost_;
};

} // namespace mbb
