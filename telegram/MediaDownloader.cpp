#include "telegram/MediaDownloader.h"

#include "infra/AudioConverter.h"
#include "infra/Base64.h"
#include "infra/FileUtils.h"
#include "infra/HttpClient.h"
#include "telegram/TelegramApi.h"

#include <fmt/core.h>

#include <stdexcept>

namespace mbb
{

MediaDownloader::MediaDownloader(std::shared_ptr<TelegramApi> api, std::shared_ptr<HttpClient> http)
    : api_(std::move(api)), http_(std::move(http))
{
}

std::string MediaDownloader::DownloadFileAsBase64(const std::string& fileId)
{
    auto fileInfo = api_->GetFileInfo(fileId);

    if (fileInfo.fileSize > 0 && static_cast<size_t>(fileInfo.fileSize) > kMaxImageSize)
    {
        throw std::runtime_error(
            fmt::format("File size {} bytes exceeds maximum allowed {} bytes", fileInfo.fileSize, kMaxImageSize));
    }

    auto data = http_->Get(fileInfo.url);
    return Base64::Encode(data);
}

std::string MediaDownloader::DownloadVoiceAsBase64(const std::string& fileId)
{
    // Get file info and validate size
    auto fileInfo = api_->GetFileInfo(fileId);

    if (fileInfo.fileSize > 0 && static_cast<size_t>(fileInfo.fileSize) > kMaxAudioSize)
    {
        throw std::runtime_error(
            fmt::format("Audio file size {} bytes exceeds maximum allowed {} bytes", fileInfo.fileSize, kMaxAudioSize));
    }

    // Download OGG file
    auto oggData = http_->Get(fileInfo.url);

    // Save to temp file
    ScopedTempFile oggFile(FileUtils::GetTempFilePath(".ogg"));
    FileUtils::WriteBinaryFile(oggFile.Path(), oggData);

    // Convert OGG to WAV
    auto wavPath = AudioConverter::ConvertOggToWav(oggFile.Path());
    ScopedTempFile wavFile(wavPath);

    // Read WAV and encode to base64
    auto wavData = FileUtils::ReadBinaryFile(wavFile.Path());
    auto base64 = Base64::Encode(wavData);

    return base64;
}

TgBot::InputFile::Ptr MediaDownloader::PrepareVoiceFromBase64(const std::string& base64Wav)
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

    return inputFile;
}

} // namespace mbb
