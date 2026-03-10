#include "bot/BotApp.h"

#include "ai/GoogleService.h"
#include "ai/OpenAiService.h"
#include "ai/XAiService.h"
#include "bot/CommandHandlers.h"
#include "bot/UserState.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/Storage.h"
#include "infra/Env.h"
#include "infra/HttpClient.h"
#include "infra/TaskQueue.h"
#include "telegram/IMessageWorker.h"
#include "telegram/MediaDownloader.h"
#include "telegram/MessageWorker.h"
#include "telegram/TelegramApi.h"

#include <fmt/core.h>
#include <fmt/std.h>
#include <tgbot/tgbot.h>

#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace mbb
{

namespace
{
std::atomic<bool>* g_running = nullptr;

#if defined(_WIN32)
BOOL WINAPI ConsoleHandler(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT && g_running)
    {
        Logger::Info("CTRL+C received. Exiting...");
        *g_running = false;
    }
    return TRUE;
}
#else
void SignalHandler(int signal)
{
    Logger::Info(fmt::format("Signal {} received. Exiting...", signal));
    if (g_running)
    {
        *g_running = false;
    }
}
#endif
} // anonymous namespace

BotApp::BotApp() = default;

BotApp::~BotApp()
{
    Stop();
}

void BotApp::Stop()
{
    running_ = false;
    if (taskQueue_)
    {
        taskQueue_->Stop();
    }
    if (messageWorker_)
    {
        messageWorker_->Stop();
    }
}

int BotApp::Run()
{
#if defined(_WIN32)
    SetConsoleOutputCP(65001);
    setlocale(LC_ALL, ".utf8");
#endif

    // Initialize configuration
    try
    {
        Config::Init();
        running_ = true;
    }
    catch (const std::runtime_error& e)
    {
        Logger::Error(fmt::format("Config initialization failed: {}", e.what()));
        return EXIT_FAILURE;
    }

    // Initialize components
    try
    {
        InitializeComponents();
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Component initialization failed: {}", e.what()));
        return EXIT_FAILURE;
    }

    // Register handlers and commands
    RegisterHandlers();
    SetBotCommands();

    // Setup signal handlers
    g_running = &running_;
#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#endif

    // Start bot
    bot_->getApi().deleteWebhook();

    Logger::Info(fmt::format("{} ({}) started. Thread ID: {}", Config::GetBotName(), bot_->getApi().getMe()->username,
                             std::this_thread::get_id()));

    constexpr std::int32_t kPollLimit = 100;
    constexpr std::int32_t kPollTimeoutSec = 3;
    std::int32_t lastUpdateId = 0;

    while (running_)
    {
        try
        {
            auto updates = bot_->getApi().getUpdates(lastUpdateId, kPollLimit, kPollTimeoutSec, nullptr);
            for (auto& update : updates)
            {
                if (!running_)
                {
                    break;
                }
                if (update->updateId >= lastUpdateId)
                {
                    lastUpdateId = update->updateId + 1;
                }
                bot_->getEventHandler().handleUpdate(update);
            }
        }
        catch (const std::exception& e)
        {
            if (running_)
            {
                Logger::Error(fmt::format("Polling exception: {}. Continuing...", e.what()));
            }
        }
    }

    Logger::Info(fmt::format("{} ({}) stopped", Config::GetBotName(), bot_->getApi().getMe()->username));
    return EXIT_SUCCESS;
}

void BotApp::InitializeComponents()
{
    // Storage
    storage_ = std::make_shared<Storage>(Config::GetDbPath());

    // HTTP client for AI services
    http_ = std::make_shared<HttpClient>();

    // Task queue for async work
    size_t workerCount = std::thread::hardware_concurrency();
    const auto workerEnv = Env::GetOptional("MYBUDDYBOT_WORKER_THREADS");
    if (!workerEnv.empty())
    {
        try
        {
            size_t parsed = static_cast<size_t>(std::stoul(workerEnv));
            if (parsed > 0)
            {
                workerCount = parsed;
            }
        }
        catch (...)
        {
            Logger::Error("Invalid MYBUDDYBOT_WORKER_THREADS value. Using default worker count.");
        }
    }
    if (workerCount == 0)
    {
        workerCount = 1;
    }
    taskQueue_ = std::make_shared<TaskQueue>(workerCount);

    // Telegram bot
    tgBotHttp_ = std::make_unique<TgBot::CurlHttpClient>();
    bot_ = std::make_shared<TgBot::Bot>(Config::GetBotToken(), *tgBotHttp_);

    // Telegram API wrapper
    telegramApi_ = std::make_shared<TelegramApi>(bot_);

    // Media downloader
    mediaDownloader_ = std::make_shared<MediaDownloader>(telegramApi_, http_);

    // User state
    userState_ = std::make_shared<UserState>();
    userState_->LoadFromFile(Config::GetStatePath());

    messageWorker_ = std::make_shared<MessageWorker>();

    // AI services
    if (Config::IsProviderEnabled(AiProvider::OpenAI))
    {
        openAiService_ = std::make_shared<OpenAiService>(messageWorker_);
    }
    if (Config::IsProviderEnabled(AiProvider::XAI))
    {
        xAiService_ = std::make_shared<XAiService>(messageWorker_);
    }
    if (Config::IsProviderEnabled(AiProvider::Google))
    {
        googleService_ = std::make_shared<GoogleService>(messageWorker_);
    }

    // Command handlers
    handlers_ = std::make_shared<CommandHandlers>(telegramApi_, mediaDownloader_, storage_, userState_, taskQueue_,
                                                  openAiService_, xAiService_, googleService_);

    // Start message worker for streaming responses
    messageWorker_->Start(telegramApi_);
}

void BotApp::RegisterHandlers()
{
    // Any message - track it
    bot_->getEvents().onAnyMessage(
        [this](const TgBot::Message::Ptr& message) { handlers_->HandleAnyMessage(message); });

    // Commands
    bot_->getEvents().onCommand("start",
                                [this](const TgBot::Message::Ptr& message) { handlers_->HandleStart(message); });

    bot_->getEvents().onCommand("clear",
                                [this](const TgBot::Message::Ptr& message) { handlers_->HandleClear(message); });

    bot_->getEvents().onCommand(
        "switch_provider", [this](const TgBot::Message::Ptr& message) { handlers_->HandleSwitchProvider(message); });

    bot_->getEvents().onCommand("switch_model",
                                [this](const TgBot::Message::Ptr& message) { handlers_->HandleSwitchModel(message); });

    bot_->getEvents().onCommand("audio",
                                [this](const TgBot::Message::Ptr& message) { handlers_->HandleAudio(message); });

    bot_->getEvents().onCommand("image",
                                [this](const TgBot::Message::Ptr& message) { handlers_->HandleImage(message); });

    bot_->getEvents().onCommand("system",
                                [this](const TgBot::Message::Ptr& message) { handlers_->HandleSystem(message); });

    bot_->getEvents().onCommand("health",
                                [this](const TgBot::Message::Ptr& message) { handlers_->HandleHealth(message); });

    // Non-command messages
    bot_->getEvents().onNonCommandMessage(
        [this](const TgBot::Message::Ptr& message) { handlers_->HandleNonCommandMessage(message); });
}

void BotApp::SetBotCommands()
{
    std::vector<TgBot::BotCommand::Ptr> commands;

    auto cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "start";
    cmd->description = "Start the bot";
    commands.push_back(cmd);

    cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "clear";
    cmd->description = "Clear the chat history";
    commands.push_back(cmd);

    cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "switch_provider";
    cmd->description = "Switch between OpenAI, xAI and Google";
    commands.push_back(cmd);

    cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "switch_model";
    cmd->description = "Toggle between primary and secondary AI model";
    commands.push_back(cmd);

    cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "audio";
    cmd->description = "Enable/disable audio responses";
    commands.push_back(cmd);

    cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "image";
    cmd->description = "Generate an image";
    commands.push_back(cmd);

    cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "system";
    cmd->description = "Set a system prompt to customize AI behavior";
    commands.push_back(cmd);

    cmd = std::make_shared<TgBot::BotCommand>();
    cmd->command = "health";
    cmd->description = "Show health and runtime configuration";
    commands.push_back(cmd);

    bot_->getApi().setMyCommands(commands);

    // Log registered commands
    auto cmds = bot_->getApi().getMyCommands();
    for (const auto& c : cmds)
    {
        Logger::Info(fmt::format("cmd: {} -> {}", c->command, c->description));
    }
}

} // namespace mbb
