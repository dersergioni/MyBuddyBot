#include "telegram/TelegramApi.h"

#include "core/Logger.h"
#include "infra/AudioConverter.h"
#include "infra/Base64.h"
#include "infra/FileUtils.h"

#include <fmt/core.h>

namespace mbb
{

TelegramApi::TelegramApi(std::shared_ptr<TgBot::Bot> bot) : bot_(std::move(bot))
{
}

TgBot::Message::Ptr TelegramApi::SendMessage(int64_t chatId,
                                             int32_t threadId,
                                             const std::string& text,
                                             const std::string& parseMode)
{
    try
    {
        std::lock_guard<std::mutex> lock(apiMutex_);
        return bot_->getApi().sendMessage(chatId, text,
                                          nullptr, // linkPreviewOptions
                                          nullptr, // replyParameters
                                          nullptr, // replyMarkup
                                          parseMode,
                                          true, // disableNotification
                                          {},   // entities
                                          threadId,
                                          false, // protectContent
                                          ""     // businessConnectionId
        );
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Failed to send message: {}", e.what()));
        return nullptr;
    }
}

TgBot::Message::Ptr TelegramApi::SendPhoto(int64_t chatId,
                                           int32_t threadId,
                                           const std::string& photoUrl,
                                           const std::string& caption)
{
    try
    {
        std::lock_guard<std::mutex> lock(apiMutex_);
        return bot_->getApi().sendPhoto(chatId, photoUrl, caption,
                                        nullptr, // replyParameters
                                        nullptr, // replyMarkup
                                        "",      // parseMode
                                        false,   // disableNotification
                                        {},      // captionEntities
                                        threadId,
                                        false, // protectContent
                                        false, // hasSpoiler
                                        ""     // businessConnectionId
        );
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Failed to send photo: {}", e.what()));
        return nullptr;
    }
}

TgBot::Message::Ptr TelegramApi::SendPhoto(int64_t chatId,
                                           int32_t threadId,
                                           TgBot::InputFile::Ptr photo,
                                           const std::string& caption)
{
    try
    {
        std::lock_guard<std::mutex> lock(apiMutex_);
        return bot_->getApi().sendPhoto(chatId, photo, caption,
                                        nullptr, // replyParameters
                                        nullptr, // replyMarkup
                                        "",      // parseMode
                                        false,   // disableNotification
                                        {},      // captionEntities
                                        threadId,
                                        false, // protectContent
                                        false, // hasSpoiler
                                        ""     // businessConnectionId
        );
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Failed to send photo: {}", e.what()));
        return nullptr;
    }
}

TgBot::Message::Ptr TelegramApi::SendVoice(int64_t chatId, int32_t threadId, TgBot::InputFile::Ptr voice)
{
    try
    {
        std::lock_guard<std::mutex> lock(apiMutex_);
        return bot_->getApi().sendVoice(chatId, voice,
                                        "",      // caption
                                        0,       // duration
                                        nullptr, // replyParameters
                                        nullptr, // replyMarkup
                                        "",      // parseMode
                                        false,   // disableNotification
                                        {},      // captionEntities
                                        threadId,
                                        false, // protectContent
                                        ""     // businessConnectionId
        );
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Failed to send voice: {}", e.what()));
        return nullptr;
    }
}

TgBot::Message::Ptr TelegramApi::SendVoice(int64_t chatId, int32_t threadId, const std::string& base64Wav)
{
    try
    {
        // Decode base64 to raw WAV data
        auto wavData = Base64::Decode(base64Wav);

        // Save to temp file
        ScopedTempFile wavFile(FileUtils::GetTempFilePath(".wav"));
        FileUtils::WriteBinaryFile(wavFile.Path(), wavData);

        // Convert WAV to OGG (Telegram format)
        auto oggPath = AudioConverter::ConvertWavToOgg(wavFile.Path());
        ScopedTempFile oggFile(oggPath);

        // Read OGG file
        auto oggData = FileUtils::ReadBinaryFile(oggFile.Path());

        // Create InputFile
        auto inputFile = std::make_shared<TgBot::InputFile>();
        inputFile->data = std::string(oggData.begin(), oggData.end());
        inputFile->fileName = "voice.ogg";
        inputFile->mimeType = "audio/ogg";

        return SendVoice(chatId, threadId, inputFile);
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Failed to send voice from base64: {}", e.what()));
        return nullptr;
    }
}

TgBot::Message::Ptr TelegramApi::EditMessage(int64_t chatId,
                                             int32_t messageId,
                                             const std::string& text,
                                             const std::string& parseMode)
{
    try
    {
        std::lock_guard<std::mutex> lock(apiMutex_);
        return bot_->getApi().editMessageText(text, chatId, messageId,
                                              "", // inlineMessageId
                                              parseMode,
                                              nullptr, // linkPreviewOptions
                                              nullptr  // replyMarkup
        );
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Failed to edit message: {}", e.what()));
        return nullptr;
    }
}

bool TelegramApi::DeleteMessage(int64_t chatId, int32_t messageId)
{
    try
    {
        std::lock_guard<std::mutex> lock(apiMutex_);
        return bot_->getApi().deleteMessage(chatId, messageId);
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("Failed to delete message {}: {}", messageId, e.what()));
        return false;
    }
}

std::string TelegramApi::GetFileUrl(const std::string& fileId)
{
    std::lock_guard<std::mutex> lock(apiMutex_);
    auto file = bot_->getApi().getFile(fileId);
    return fmt::format("https://api.telegram.org/file/bot{}/{}", bot_->getToken(), file->filePath);
}

FileInfo TelegramApi::GetFileInfo(const std::string& fileId)
{
    std::lock_guard<std::mutex> lock(apiMutex_);
    auto file = bot_->getApi().getFile(fileId);
    return FileInfo{.url = fmt::format("https://api.telegram.org/file/bot{}/{}", bot_->getToken(), file->filePath),
                    .fileSize = file->fileSize};
}

} // namespace mbb
