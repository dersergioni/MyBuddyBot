#pragma once

#include <tgbot/tgbot.h>

#include <cstddef>
#include <memory>
#include <string>

namespace mbb
{

class TelegramApi;
class HttpClient;

// Maximum file sizes for downloads
constexpr size_t kMaxImageSize = 20 * 1024 * 1024; // 20 MB
constexpr size_t kMaxAudioSize = 25 * 1024 * 1024; // 25 MB

class MediaDownloader
{
  public:
    MediaDownloader(std::shared_ptr<TelegramApi> api, std::shared_ptr<HttpClient> http);

    // Download any file and return as base64
    [[nodiscard]] std::string DownloadFileAsBase64(const std::string& fileId);

    // Download voice file, convert ogg→wav, return as base64
    [[nodiscard]] std::string DownloadVoiceAsBase64(const std::string& fileId);

    // Prepare voice message from base64 wav data (wav→ogg conversion)
    [[nodiscard]] TgBot::InputFile::Ptr PrepareVoiceFromBase64(const std::string& base64Wav);

  private:
    std::shared_ptr<TelegramApi> api_;
    std::shared_ptr<HttpClient> http_;
};

} // namespace mbb
