#include "AudioConverter.h"

#include <fmt/format.h>

#include <cstdlib>
#include <stdexcept>

namespace mbb
{

std::filesystem::path AudioConverter::ConvertOggToWav(const std::filesystem::path& oggPath)
{
    auto wavPath = oggPath;
    wavPath.replace_extension(".wav");

    // Quote paths to prevent shell injection
    std::string cmd = fmt::format("ffmpeg -y -i \"{}\" \"{}\"", oggPath.string(), wavPath.string());

    int result = std::system(cmd.c_str());
    if (result != 0)
    {
        throw std::runtime_error(
            fmt::format("Failed to convert OGG to WAV: {} (exit code: {})", oggPath.string(), result));
    }

    return wavPath;
}

std::filesystem::path AudioConverter::ConvertWavToOgg(const std::filesystem::path& wavPath)
{
    auto oggPath = wavPath;
    oggPath.replace_extension(".ogg");

    // Telegram voice message requirements:
    // -c:a libopus - Opus codec
    // -b:a 48k - bitrate (16k-128k, Telegram recommends 16-64k)
    // -ar 48000 - sample rate (Telegram requires 48000 Hz)
    // -ac 1 - mono audio (Telegram requires mono)
    // -vn - remove video track

    std::string cmd = fmt::format("ffmpeg -y -i \"{}\" -c:a libopus -b:a 48k -ar 48000 -vn -ac 1 \"{}\"",
                                  wavPath.string(), oggPath.string());

    int result = std::system(cmd.c_str());
    if (result != 0)
    {
        throw std::runtime_error(
            fmt::format("Failed to convert WAV to OGG: {} (exit code: {})", wavPath.string(), result));
    }

    return oggPath;
}

} // namespace mbb
