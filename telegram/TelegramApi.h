#pragma once

#include <tgbot/tgbot.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace mbb
{

struct FileInfo
{
    std::string url;
    int64_t fileSize = 0;
};

class TelegramApi
{
  public:
    explicit TelegramApi(std::shared_ptr<TgBot::Bot> bot);

    // Send text message
    TgBot::Message::Ptr SendMessage(int64_t chatId,
                                    int32_t threadId,
                                    const std::string& text,
                                    const std::string& parseMode = "HTML");

    // Send text message with an inline URL button
    TgBot::Message::Ptr SendMessageWithUrlButton(int64_t chatId,
                                                 int32_t threadId,
                                                 const std::string& text,
                                                 const std::string& buttonLabel,
                                                 const std::string& url);

    // Send photo by URL
    TgBot::Message::Ptr SendPhoto(int64_t chatId,
                                  int32_t threadId,
                                  const std::string& photoUrl,
                                  const std::string& caption = "");

    // Send photo from InputFile
    TgBot::Message::Ptr SendPhoto(int64_t chatId,
                                  int32_t threadId,
                                  TgBot::InputFile::Ptr photo,
                                  const std::string& caption = "");

    // Send voice message from InputFile
    TgBot::Message::Ptr SendVoice(int64_t chatId, int32_t threadId, TgBot::InputFile::Ptr voice);

    // Send voice message from base64 WAV (converts to OGG internally)
    TgBot::Message::Ptr SendVoice(int64_t chatId, int32_t threadId, const std::string& base64Wav);

    // Edit existing message
    TgBot::Message::Ptr EditMessage(int64_t chatId,
                                    int32_t messageId,
                                    const std::string& text,
                                    const std::string& parseMode = "HTML");

    // Delete message
    bool DeleteMessage(int64_t chatId, int32_t messageId);

    // Get file download URL
    [[nodiscard]] std::string GetFileUrl(const std::string& fileId);

    // Get file info including URL and size
    [[nodiscard]] FileInfo GetFileInfo(const std::string& fileId);

  private:
    std::shared_ptr<TgBot::Bot> bot_;
    mutable std::mutex apiMutex_;
};

} // namespace mbb
