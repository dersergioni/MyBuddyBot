#include "bot/CommandHandlers.h"

#include "ai/IAiService.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/Storage.h"
#include "infra/Base64.h"
#include "infra/StringUtils.h"
#include "infra/TaskQueue.h"
#include "telegram/MediaDownloader.h"
#include "telegram/TelegramApi.h"

#include <fmt/core.h>

#include <algorithm>
#include <thread>
#include <unordered_set>
#include <vector>

namespace mbb
{

namespace
{
#ifndef MYBUDDYBOT_VERSION
#define MYBUDDYBOT_VERSION "dev"
#endif

#ifndef MYBUDDYBOT_GIT_COMMIT
#define MYBUDDYBOT_GIT_COMMIT "unknown"
#endif

constexpr const char* kUnauthorizedMessage = "Sorry, you are not authorized to use this bot.";

const char* GetProviderName(AiProvider provider)
{
    switch (provider)
    {
    case AiProvider::OpenAI:
        return "OpenAI ChatGPT";
    case AiProvider::XAI:
        return "xAI Grok";
    case AiProvider::Google:
        return "Google Gemini";
    }
    return "Unknown";
}

const char* GetProviderStatus(AiProvider provider)
{
    return Config::IsProviderEnabled(provider) ? "enabled" : "disabled";
}

const char* GetModelSelectorName(ModelSelector selector)
{
    switch (selector)
    {
    case ModelSelector::Primary:
        return "primary";
    case ModelSelector::Secondary:
        return "secondary";
    case ModelSelector::Image:
        return "image";
    case ModelSelector::Audio:
        return "audio";
    }
    return "unknown";
}

int64_t ExtractUserId(const TgBot::Message::Ptr& message)
{
    if (message != nullptr && message->from != nullptr)
    {
        return message->from->id;
    }
    return 0;
}

std::string ExtractUsername(const TgBot::Message::Ptr& message)
{
    if (message != nullptr && message->from != nullptr)
    {
        return message->from->username;
    }
    return "";
}

std::string UsernameForStorage(const TgBot::Message::Ptr& message)
{
    const std::string username = ExtractUsername(message);
    if (!username.empty())
    {
        return username;
    }
    return fmt::format("id_{}", ExtractUserId(message));
}

std::string JoinIdSet(const std::unordered_set<int64_t>& values)
{
    if (values.empty())
    {
        return "(empty)";
    }

    std::vector<int64_t> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());

    std::string out = std::to_string(sorted.front());
    for (size_t i = 1; i < sorted.size(); ++i)
    {
        out += "," + std::to_string(sorted[i]);
    }
    return out;
}

std::string JoinStringSet(const std::unordered_set<std::string>& values)
{
    if (values.empty())
    {
        return "(empty)";
    }

    std::vector<std::string> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());

    std::string out = sorted.front();
    for (size_t i = 1; i < sorted.size(); ++i)
    {
        out += "," + sorted[i];
    }
    return out;
}

// Select optimal photo size for vision (not too large to exceed token limits)
const std::string& SelectPhotoForVision(const std::vector<TgBot::PhotoSize::Ptr>& photos)
{
    // Target ~1280px width for good balance between quality and token usage
    constexpr int32_t kTargetWidth = 1280;

    const TgBot::PhotoSize::Ptr* best = &photos.back();
    int32_t bestDiff = std::abs(photos.back()->width - kTargetWidth);

    for (const auto& photo : photos)
    {
        int32_t diff = std::abs(photo->width - kTargetWidth);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            best = &photo;
        }
    }

    return (*best)->fileId;
}
} // anonymous namespace

CommandHandlers::CommandHandlers(std::shared_ptr<TelegramApi> api,
                                 std::shared_ptr<MediaDownloader> downloader,
                                 std::shared_ptr<Storage> storage,
                                 std::shared_ptr<UserState> state,
                                 std::shared_ptr<TaskQueue> taskQueue,
                                 std::shared_ptr<IAiService> openAi,
                                 std::shared_ptr<IAiService> xAi,
                                 std::shared_ptr<IAiService> google)
    : api_(std::move(api)), downloader_(std::move(downloader)), storage_(std::move(storage)), state_(std::move(state)),
      taskQueue_(std::move(taskQueue)), openAi_(std::move(openAi)), xAi_(std::move(xAi)), google_(std::move(google))
{
}

// =============================================================================
// Command Handlers
// =============================================================================

void CommandHandlers::HandleStart(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message), "start");
    LogCommandInvocation(message, "/start");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    api_->SendMessage(message->chat->id, message->messageThreadId,
                      fmt::format("{} welcomes you!", Config::GetBotName()));
}

void CommandHandlers::HandleClear(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message),
                                  "clearhistory");
    LogCommandInvocation(message, "/clear");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    const ChatKey key = {message->chat->id, message->messageThreadId};

    auto ids = storage_->SelectMsgIds(message->chat->id, message->messageThreadId);
    std::for_each(ids.cbegin(), ids.cend(), [this, &message](int32_t id) {
        storage_->DeleteMsgId(message->chat->id, message->messageThreadId, id);
    });

    state_->Clear(key);

    Logger::Info(fmt::format("Clear history for chat: {} thread: {}", message->chat->id, message->messageThreadId));

    auto ai = GetCurrentAi(key);
    std::string modelName = ai ? ai->GetModelName(state_->GetModelSelector(key)) : "unknown";
    api_->SendMessage(
        message->chat->id, message->messageThreadId,
        fmt::format("--- Clearing history for {} ({}) ---", GetProviderName(state_->GetAiProvider(key)), modelName));
}

void CommandHandlers::HandleSwitchProvider(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message),
                                  "switch_provider");
    LogCommandInvocation(message, "/switch_provider");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    const ChatKey key = {message->chat->id, message->messageThreadId};

    const auto& enabledProviders = Config::GetEnabledProviders();
    if (enabledProviders.empty())
    {
        api_->SendMessage(message->chat->id, message->messageThreadId, "No AI providers are configured.");
        return;
    }

    auto current = state_->GetAiProvider(key);
    auto it = std::find(enabledProviders.begin(), enabledProviders.end(), current);
    if (it == enabledProviders.end())
    {
        current = enabledProviders.front();
    }
    else
    {
        const size_t idx = static_cast<size_t>(std::distance(enabledProviders.begin(), it));
        current = enabledProviders[(idx + 1) % enabledProviders.size()];
    }

    state_->SetAiProvider(key, current);

    api_->SendMessage(message->chat->id, message->messageThreadId,
                      fmt::format("AI provider switched to {}", GetProviderName(current)));
}

void CommandHandlers::HandleSwitchModel(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message),
                                  "switchmodel");
    LogCommandInvocation(message, "/switch_model");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    const ChatKey key = {message->chat->id, message->messageThreadId};

    state_->ToggleModelSelector(key);
    auto selector = state_->GetModelSelector(key);
    auto ai = GetCurrentAi(key);

    std::string label = (selector == ModelSelector::Secondary) ? "secondary" : "primary";
    std::string modelName = ai ? ai->GetModelName(selector) : "unknown";

    api_->SendMessage(message->chat->id, message->messageThreadId,
                      fmt::format("Model switched to {} ({})", label, modelName));
}

void CommandHandlers::HandleAudio(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message), "audio");
    LogCommandInvocation(message, "/audio");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    const ChatKey key = {message->chat->id, message->messageThreadId};

    state_->ToggleAudioResponse(key);
    bool enabled = state_->GetAudioResponse(key);

    api_->SendMessage(message->chat->id, message->messageThreadId,
                      fmt::format("Audio response is now {}", enabled ? "enabled" : "disabled"));
}

void CommandHandlers::HandleImage(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message), "image");
    LogCommandInvocation(message, "/image");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    const ChatKey key = {message->chat->id, message->messageThreadId};
    api_->SendMessage(message->chat->id, message->messageThreadId, "Ok. Send me a prompt for image generation:");
    state_->SetDialogMode(key, DialogMode::ImageGeneration);
}

void CommandHandlers::HandleSystem(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message), "system");
    LogCommandInvocation(message, "/system");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    // Extract text after "/system" or "/system@BotName" command
    std::string text;
    auto spacePos = message->text.find(' ');
    if (spacePos != std::string::npos)
    {
        text = message->text.substr(spacePos + 1);
    }

    if (text.empty())
    {
        // Show current system prompt
        auto systemMsg = storage_->GetSystemMessage(message->chat->id, message->messageThreadId);
        if (systemMsg.empty())
        {
            api_->SendMessage(message->chat->id, message->messageThreadId, "No system prompt is set.");
        }
        else
        {
            api_->SendMessage(message->chat->id, message->messageThreadId,
                              fmt::format("Current system prompt:\n{}", systemMsg));
        }
    }
    else if (text == "off")
    {
        // Clear system prompt
        storage_->ClearSystemMessage(message->chat->id, message->messageThreadId);
        api_->SendMessage(message->chat->id, message->messageThreadId, "System prompt cleared.");
    }
    else
    {
        // Set new system prompt
        storage_->InsertSystemMessage(message->chat->id, message->messageThreadId, UsernameForStorage(message),
                                      Storage::Role::System, text);
        api_->SendMessage(message->chat->id, message->messageThreadId, fmt::format("System prompt set:\n{}", text));
    }
}

void CommandHandlers::HandleHealth(const TgBot::Message::Ptr& message)
{
    storage_->InsertSystemCommand(message->chat->id, message->messageThreadId, UsernameForStorage(message), "health");
    LogCommandInvocation(message, "/health");

    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    const ChatKey key = {message->chat->id, message->messageThreadId};

    auto provider = state_->GetAiProvider(key);
    if (!Config::IsProviderEnabled(provider))
    {
        provider = Config::GetDefaultProvider();
    }

    const auto selector = state_->GetModelSelector(key);
    const auto ai = GetCurrentAi(key);
    const std::string modelName = ai ? ai->GetModelName(selector) : "unknown";

    const std::string health = fmt::format("Health check: OK\n"
                                           "bot name: {}\n"
                                           "version: {}\n"
                                           "commit: {}\n"
                                           "debug mode: {}\n"
                                           "db path: {}\n"
                                           "state path: {}\n"
                                           "providers: OpenAI={}, xAI={}, Google={}\n"
                                           "active provider: {}\n"
                                           "active model: {} ({})",
                                           Config::GetBotName(), MYBUDDYBOT_VERSION, MYBUDDYBOT_GIT_COMMIT,
                                           Config::IsDebugMode() ? "on" : "off", Config::GetDbPath().string(),
                                           Config::GetStatePath().string(), GetProviderStatus(AiProvider::OpenAI),
                                           GetProviderStatus(AiProvider::XAI), GetProviderStatus(AiProvider::Google),
                                           GetProviderName(provider), GetModelSelectorName(selector), modelName);

    std::string response = health;
    if (IsAdminMessageUser(message))
    {
        response += fmt::format("\n\naccess control:\n"
                                "allowlist ids: {}\n"
                                "allowlist usernames: {}\n"
                                "blocklist ids: {}\n"
                                "blocklist usernames: {}\n"
                                "admin ids: {}",
                                JoinIdSet(Config::GetAllowlistIds()), JoinStringSet(Config::GetAllowlistUsernames()),
                                JoinIdSet(Config::GetBlocklistIds()), JoinStringSet(Config::GetBlocklistUsernames()),
                                JoinIdSet(Config::GetAdminIds()));
    }

    api_->SendMessage(message->chat->id, message->messageThreadId, response);
}

// =============================================================================
// Message Handlers
// =============================================================================

void CommandHandlers::HandleAnyMessage(const TgBot::Message::Ptr& message)
{
    Logger::Debug(fmt::format("Incoming message: chat_id: {}, chat_title: {}, thread_id: {}, message_id: {}",
                              message->chat->id, message->chat->title, message->messageThreadId, message->messageId));

    if (!IsAuthorizedMessageUser(message))
    {
        return;
    }

    storage_->InsertMsgId(message->chat->id, message->messageThreadId, message->messageId);
}

void CommandHandlers::HandleNonCommandMessage(const TgBot::Message::Ptr& message)
{
    if (!IsAuthorizedMessageUser(message))
    {
        ReplyUnauthorized(message);
        return;
    }

    const ChatKey key = {message->chat->id, message->messageThreadId};
    const auto dialogMode = state_->GetDialogMode(key);
    auto ai = GetCurrentAi(key);
    if (!ai)
    {
        api_->SendMessage(message->chat->id, message->messageThreadId, "No AI providers are configured.");
        return;
    }

    switch (dialogMode)
    {
    case DialogMode::ImageGeneration:
        // Photo without text → accumulate as reference image
        if (!message->photo.empty() && message->text.empty() && message->caption.empty())
        {
            try
            {
                auto imageBase64 = downloader_->DownloadFileAsBase64(message->photo.back()->fileId);
                state_->AddPendingImage(key, std::move(imageBase64));
                api_->SendMessage(message->chat->id, message->messageThreadId,
                                  "Reference image added. Send more images or text prompt to generate.");
            }
            catch (const std::exception& e)
            {
                Logger::Debug(fmt::format("Failed to download reference image: {}", e.what()));
            }
        }
        // Text (with or without photo) → generate with all accumulated images
        else if (!message->text.empty() || !message->caption.empty())
        {
            ProcessImageAsync(message, ai);
            state_->SetDialogMode(key, DialogMode::None);
        }
        break;

    case DialogMode::None:
        if (message->voice != nullptr)
        {
            ProcessVoiceAsync(message, ai);
        }
        // Photo without text → accumulate for vision (unless from already processed media group)
        else if (!message->photo.empty() && message->text.empty() && message->caption.empty())
        {
            // Ignore photos from already processed media group
            if (!message->mediaGroupId.empty() && state_->IsFromLastProcessedMediaGroup(key, message->mediaGroupId))
            {
                Logger::Debug(
                    fmt::format("Ignoring photo from already processed media group: {}", message->mediaGroupId));
                break;
            }

            try
            {
                auto imageBase64 = downloader_->DownloadFileAsBase64(SelectPhotoForVision(message->photo));
                state_->AddPendingImage(key, std::move(imageBase64));
                api_->SendMessage(message->chat->id, message->messageThreadId,
                                  "Image added. Send more images or text to process.");
            }
            catch (const std::exception& e)
            {
                Logger::Debug(fmt::format("Failed to download image: {}", e.what()));
            }
        }
        // Text (with or without photo) → process with all accumulated images
        else if (!message->text.empty() || !message->caption.empty())
        {
            ProcessTextAsync(message, ai);
        }
        break;
    }
}

bool CommandHandlers::IsAuthorizedMessageUser(const TgBot::Message::Ptr& message) const
{
    return Config::IsUserAuthorized(ExtractUserId(message), StringUtils::NormalizeUsername(ExtractUsername(message)));
}

bool CommandHandlers::IsAdminMessageUser(const TgBot::Message::Ptr& message) const
{
    return Config::IsAdminUser(ExtractUserId(message));
}

void CommandHandlers::ReplyUnauthorized(const TgBot::Message::Ptr& message) const
{
    api_->SendMessage(message->chat->id, message->messageThreadId, kUnauthorizedMessage);
    Logger::Info(fmt::format("Unauthorized message denied: user_id={} username='{}' chat_id={} thread_id={}",
                             ExtractUserId(message), ExtractUsername(message), message->chat->id,
                             message->messageThreadId));
}

void CommandHandlers::LogCommandInvocation(const TgBot::Message::Ptr& message, const std::string& command) const
{
    Logger::Info(fmt::format("Command {} received: user_id={} username='{}' chat_id={} thread_id={}", command,
                             ExtractUserId(message), ExtractUsername(message), message->chat->id,
                             message->messageThreadId));
}

std::shared_ptr<IAiService> CommandHandlers::GetCurrentAi(const ChatKey& key) const
{
    auto provider = state_->GetAiProvider(key);
    if (!Config::IsProviderEnabled(provider))
    {
        provider = Config::GetDefaultProvider();
    }

    switch (provider)
    {
    case AiProvider::OpenAI:
        return openAi_;
    case AiProvider::XAI:
        return xAi_;
    case AiProvider::Google:
        return google_;
    }

    for (auto enabled : Config::GetEnabledProviders())
    {
        switch (enabled)
        {
        case AiProvider::OpenAI:
            if (openAi_)
                return openAi_;
            break;
        case AiProvider::XAI:
            if (xAi_)
                return xAi_;
            break;
        case AiProvider::Google:
            if (google_)
                return google_;
            break;
        }
    }

    return nullptr;
}

Storage::Role CommandHandlers::GetAssistantRole(const ChatKey& key) const
{
    auto provider = state_->GetAiProvider(key);
    if (!Config::IsProviderEnabled(provider))
    {
        provider = Config::GetDefaultProvider();
    }

    switch (provider)
    {
    case AiProvider::OpenAI:
        return Storage::Role::AssistantOpenAI;
    case AiProvider::XAI:
        return Storage::Role::AssistantXAI;
    case AiProvider::Google:
        return Storage::Role::AssistantGoogle;
    }
    return Storage::Role::AssistantOpenAI;
}

// =============================================================================
// Async Processing
// =============================================================================

void CommandHandlers::ProcessTextAsync(const TgBot::Message::Ptr& message, const std::shared_ptr<IAiService>& ai)
{
    const ChatKey key = {message->chat->id, message->messageThreadId};
    bool audioResponseEnabled = state_->GetAudioResponse(key);

    // Extract text from message or caption
    std::string textPrompt = message->text.empty() ? message->caption : message->text;
    auto modelSelector = state_->GetModelSelector(key);

    // Get accumulated vision images
    std::vector<std::string> visionImagesBase64 = state_->TakePendingImages(key);

    // Add current photo if present
    if (!message->photo.empty())
    {
        try
        {
            auto imageBase64 = downloader_->DownloadFileAsBase64(SelectPhotoForVision(message->photo));
            visionImagesBase64.push_back(std::move(imageBase64));
        }
        catch (const std::exception& e)
        {
            Logger::Debug(fmt::format("Failed to download image: {}", e.what()));
        }

        // Remember media group ID to ignore other photos from the same group
        if (!message->mediaGroupId.empty())
        {
            state_->SetLastProcessedMediaGroupId(key, message->mediaGroupId);
        }
    }

    if (visionImagesBase64.empty())
    {
        Logger::Info(fmt::format("User {} sent text message", message->from->username));
    }
    else
    {
        Logger::Info(fmt::format("User {} sent vision request with {} images", message->from->username,
                                 visionImagesBase64.size()));
    }

    try
    {
        storage_->InsertChatRecord(message->chat->id, message->messageThreadId, message->from->username,
                                   Storage::Role::User, textPrompt);

        const auto chatHistory = storage_->SelectActualUserChatHistory(message->chat->id, message->messageThreadId);

        auto chatId = message->chat->id;
        auto threadId = message->messageThreadId;
        auto role = GetAssistantRole({chatId, threadId});

        auto task = [ai, api = api_, chatId, threadId, chatHistory, modelSelector, storage = storage_, role,
                     audioResponseEnabled, visionImagesBase64 = std::move(visionImagesBase64)]() {
            try
            {
                const auto response = ai->GetTextResponse(chatId, threadId, chatHistory, visionImagesBase64,
                                                          modelSelector, audioResponseEnabled);

                // Store text in history
                if (!response.text.empty())
                {
                    storage->InsertChatRecord(chatId, threadId, ai->GetModelName(modelSelector), role, response.text);
                }
                else
                {
                    Logger::Error(fmt::format("{} returned empty response (textStreamed: {})",
                                              ai->GetModelName(modelSelector), response.textStreamed));
                }

                // Send audio if present
                for (const auto& media : response.GetAudioItems())
                {
                    api->SendVoice(chatId, threadId, media.data);
                }

                Logger::Info(
                    fmt::format("{} answered ({} chars)", ai->GetModelName(modelSelector), response.text.size()));
            }
            catch (const std::exception& e)
            {
                api->SendMessage(chatId, threadId, e.what());
                Logger::Debug(fmt::format("Text processing failed: {}", e.what()));
            }
        };

        if (taskQueue_)
        {
            taskQueue_->Enqueue(std::move(task));
        }
        else
        {
            std::thread(std::move(task)).detach();
        }
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("ProcessTextAsync failed: {}", e.what()));
    }
}

void CommandHandlers::ProcessVoiceAsync(const TgBot::Message::Ptr& message, const std::shared_ptr<IAiService>& ai)
{
    const ChatKey key = {message->chat->id, message->messageThreadId};
    bool audioResponseEnabled = state_->GetAudioResponse(key);

    Logger::Info(fmt::format("User {} sent voice message", message->from->username));

    try
    {
        storage_->InsertChatRecord(message->chat->id, message->messageThreadId, message->from->username,
                                   Storage::Role::User, "User sent voice message");

        auto chatId = message->chat->id;
        auto threadId = message->messageThreadId;
        auto fileId = message->voice->fileId;
        auto role = GetAssistantRole(key);

        auto task = [ai, api = api_, storage = storage_, downloader = downloader_, chatId, threadId, fileId,
                     audioResponseEnabled, role]() {
            try
            {
                // Download and convert voice to base64 WAV
                auto voiceBase64 = downloader->DownloadVoiceAsBase64(fileId);

                auto response = ai->GetResponseFromVoice(chatId, threadId, voiceBase64, audioResponseEnabled);

                // Send audio if present
                for (const auto& media : response.GetAudioItems())
                {
                    api->SendVoice(chatId, threadId, media.data);
                }

                // Save text response to history (skip for TTS mode - no text)
                if (!audioResponseEnabled && !response.text.empty())
                {
                    storage->InsertChatRecord(chatId, threadId, ai->GetModelName(ModelSelector::Audio), role,
                                              response.text);
                }
                Logger::Info(fmt::format("{} answered", ai->GetModelName(ModelSelector::Audio)));
            }
            catch (const std::exception& e)
            {
                api->SendMessage(chatId, threadId, e.what());
                Logger::Debug(fmt::format("Voice processing failed: {}", e.what()));
            }
        };

        if (taskQueue_)
        {
            taskQueue_->Enqueue(std::move(task));
        }
        else
        {
            std::thread(std::move(task)).detach();
        }
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("ProcessVoiceAsync failed: {}", e.what()));
    }
}

void CommandHandlers::ProcessImageAsync(const TgBot::Message::Ptr& message, const std::shared_ptr<IAiService>& ai)
{
    const ChatKey key = {message->chat->id, message->messageThreadId};

    // Extract text from message or caption
    std::string prompt = message->text.empty() ? message->caption : message->text;

    // Get accumulated reference images
    std::vector<std::string> referenceImagesBase64 = state_->TakePendingImages(key);

    // Add current photo if present
    if (!message->photo.empty())
    {
        try
        {
            auto imageBase64 = downloader_->DownloadFileAsBase64(message->photo.back()->fileId);
            referenceImagesBase64.push_back(std::move(imageBase64));
        }
        catch (const std::exception& e)
        {
            Logger::Debug(fmt::format("Failed to download image: {}", e.what()));
        }
    }

    Logger::Info(fmt::format("User {} sent image generation request with {} reference images", message->from->username,
                             referenceImagesBase64.size()));

    try
    {
        storage_->InsertChatRecord(message->chat->id, message->messageThreadId, message->from->username,
                                   Storage::Role::User, prompt);

        auto chatId = message->chat->id;
        auto threadId = message->messageThreadId;
        auto role = GetAssistantRole({chatId, threadId});

        auto task = [ai, api = api_, storage = storage_, chatId, threadId, prompt, role,
                     referenceImagesBase64 = std::move(referenceImagesBase64)]() {
            try
            {
                const auto response = ai->GetImageResponse(chatId, threadId, prompt, referenceImagesBase64);

                // Send images to Telegram and store in history
                for (const auto& media : response.GetImageItems())
                {
                    if (media.type == MediaItem::Type::ImageUrl)
                    {
                        api->SendPhoto(chatId, threadId, media.data);
                        storage->InsertChatRecord(chatId, threadId, ai->GetModelName(ModelSelector::Image), role,
                                                  media.data);
                    }
                    else if (media.type == MediaItem::Type::ImageBase64)
                    {
                        auto imageData = Base64::DecodeToBytes(media.data);
                        auto inputFile = std::make_shared<TgBot::InputFile>();
                        inputFile->data = std::string(imageData.begin(), imageData.end());
                        inputFile->mimeType = media.mimeType;
                        inputFile->fileName = "image.png";
                        api->SendPhoto(chatId, threadId, inputFile);
                        storage->InsertChatRecord(chatId, threadId, ai->GetModelName(ModelSelector::Image), role,
                                                  "[generated image]");
                    }
                }

                Logger::Info(fmt::format("{} generated {} images", ai->GetModelName(ModelSelector::Image),
                                         response.media.size()));
            }
            catch (const std::exception& e)
            {
                api->SendMessage(chatId, threadId, e.what());
                Logger::Debug(fmt::format("Image generation failed: {}", e.what()));
            }
        };

        if (taskQueue_)
        {
            taskQueue_->Enqueue(std::move(task));
        }
        else
        {
            std::thread(std::move(task)).detach();
        }
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("ProcessImageAsync failed: {}", e.what()));
    }
}

} // namespace mbb
