#pragma once

#include <filesystem>

namespace mbb
{

class AudioConverter
{
  public:
    // Convert OGG audio to WAV format
    // Returns path to the created WAV file
    [[nodiscard]] static std::filesystem::path ConvertOggToWav(const std::filesystem::path& oggPath);

    // Convert WAV audio to OGG format (Opus codec for Telegram)
    // Returns path to the created OGG file
    [[nodiscard]] static std::filesystem::path ConvertWavToOgg(const std::filesystem::path& wavPath);
};

} // namespace mbb
