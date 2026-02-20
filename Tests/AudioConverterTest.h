#ifndef AUDIOCONVERTERTEST_H
#define AUDIOCONVERTERTEST_H

#include "../infra/AudioConverter.h"
#include "../infra/FileUtils.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace mbb::tests
{

class AudioConverterTest : public testing::Test
{
  protected:
    AudioConverterTest() = default;

    ~AudioConverterTest() override
    {
        // Clean up temp files
        for (const auto& path : createdFiles_)
        {
            if (std::filesystem::exists(path))
            {
                std::filesystem::remove(path);
            }
        }
    }

    void SetUp() override
    {
        // Check if ffmpeg is available
#if defined(_WIN32)
        int result = std::system("ffmpeg -version >NUL 2>&1");
#else
        int result = std::system("ffmpeg -version > /dev/null 2>&1");
#endif
        ffmpegAvailable_ = (result == 0);
    }

    // Create a minimal valid WAV file for testing
    std::filesystem::path CreateTestWavFile()
    {
        auto path = FileUtils::GetTempFilePath(".wav");
        createdFiles_.insert(path);

        // Minimal WAV header for 1 second of silence at 16000 Hz, 16-bit, mono
        // WAV format: RIFF header + fmt chunk + data chunk
        std::vector<uint8_t> wavData;

        // RIFF header
        const char* riff = "RIFF";
        wavData.insert(wavData.end(), riff, riff + 4);

        uint32_t fileSize = 44 + 32000 - 8; // header + data - 8 bytes for RIFF/size
        wavData.push_back(fileSize & 0xFF);
        wavData.push_back((fileSize >> 8) & 0xFF);
        wavData.push_back((fileSize >> 16) & 0xFF);
        wavData.push_back((fileSize >> 24) & 0xFF);

        const char* wave = "WAVE";
        wavData.insert(wavData.end(), wave, wave + 4);

        // fmt chunk
        const char* fmt = "fmt ";
        wavData.insert(wavData.end(), fmt, fmt + 4);

        uint32_t fmtSize = 16;
        wavData.push_back(fmtSize & 0xFF);
        wavData.push_back((fmtSize >> 8) & 0xFF);
        wavData.push_back((fmtSize >> 16) & 0xFF);
        wavData.push_back((fmtSize >> 24) & 0xFF);

        uint16_t audioFormat = 1; // PCM
        wavData.push_back(audioFormat & 0xFF);
        wavData.push_back((audioFormat >> 8) & 0xFF);

        uint16_t numChannels = 1; // Mono
        wavData.push_back(numChannels & 0xFF);
        wavData.push_back((numChannels >> 8) & 0xFF);

        uint32_t sampleRate = 16000;
        wavData.push_back(sampleRate & 0xFF);
        wavData.push_back((sampleRate >> 8) & 0xFF);
        wavData.push_back((sampleRate >> 16) & 0xFF);
        wavData.push_back((sampleRate >> 24) & 0xFF);

        uint32_t byteRate = 32000; // sampleRate * numChannels * bitsPerSample/8
        wavData.push_back(byteRate & 0xFF);
        wavData.push_back((byteRate >> 8) & 0xFF);
        wavData.push_back((byteRate >> 16) & 0xFF);
        wavData.push_back((byteRate >> 24) & 0xFF);

        uint16_t blockAlign = 2; // numChannels * bitsPerSample/8
        wavData.push_back(blockAlign & 0xFF);
        wavData.push_back((blockAlign >> 8) & 0xFF);

        uint16_t bitsPerSample = 16;
        wavData.push_back(bitsPerSample & 0xFF);
        wavData.push_back((bitsPerSample >> 8) & 0xFF);

        // data chunk
        const char* data = "data";
        wavData.insert(wavData.end(), data, data + 4);

        uint32_t dataSize = 32000; // 1 second of audio
        wavData.push_back(dataSize & 0xFF);
        wavData.push_back((dataSize >> 8) & 0xFF);
        wavData.push_back((dataSize >> 16) & 0xFF);
        wavData.push_back((dataSize >> 24) & 0xFF);

        // Audio data (silence)
        wavData.resize(wavData.size() + dataSize, 0);

        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(wavData.data()), wavData.size());

        return path;
    }

    bool ffmpegAvailable_ = false;
    std::set<std::filesystem::path> createdFiles_;
};

TEST_F(AudioConverterTest, FfmpegIsAvailable)
{
    // This test just documents whether ffmpeg is available in the test environment
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available, skipping audio converter tests";
    }
    SUCCEED();
}

TEST_F(AudioConverterTest, ConvertWavToOgg)
{
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available";
    }

    auto wavPath = CreateTestWavFile();
    ASSERT_TRUE(std::filesystem::exists(wavPath));

    auto oggPath = AudioConverter::ConvertWavToOgg(wavPath);
    createdFiles_.insert(oggPath);

    EXPECT_TRUE(std::filesystem::exists(oggPath));
    EXPECT_EQ(".ogg", oggPath.extension());
    EXPECT_GT(std::filesystem::file_size(oggPath), 0u);
}

TEST_F(AudioConverterTest, ConvertOggToWav)
{
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available";
    }

    // First create a WAV and convert to OGG
    auto wavPath = CreateTestWavFile();
    auto oggPath = AudioConverter::ConvertWavToOgg(wavPath);
    createdFiles_.insert(oggPath);

    ASSERT_TRUE(std::filesystem::exists(oggPath));

    // Now convert OGG back to WAV
    auto wavPath2 = AudioConverter::ConvertOggToWav(oggPath);
    createdFiles_.insert(wavPath2);

    EXPECT_TRUE(std::filesystem::exists(wavPath2));
    EXPECT_EQ(".wav", wavPath2.extension());
    EXPECT_GT(std::filesystem::file_size(wavPath2), 0u);
}

TEST_F(AudioConverterTest, ConvertWavToOggOutputPath)
{
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available";
    }

    auto wavPath = CreateTestWavFile();
    auto oggPath = AudioConverter::ConvertWavToOgg(wavPath);
    createdFiles_.insert(oggPath);

    // Output should be in same directory with .ogg extension
    EXPECT_EQ(wavPath.parent_path(), oggPath.parent_path());
    EXPECT_EQ(wavPath.stem(), oggPath.stem());
}

TEST_F(AudioConverterTest, ConvertOggToWavOutputPath)
{
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available";
    }

    auto wavPath = CreateTestWavFile();
    auto oggPath = AudioConverter::ConvertWavToOgg(wavPath);
    createdFiles_.insert(oggPath);

    auto wavPath2 = AudioConverter::ConvertOggToWav(oggPath);
    createdFiles_.insert(wavPath2);

    // Output should be in same directory with .wav extension
    EXPECT_EQ(oggPath.parent_path(), wavPath2.parent_path());
    EXPECT_EQ(oggPath.stem(), wavPath2.stem());
}

TEST_F(AudioConverterTest, ConvertNonexistentFileThrows)
{
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available";
    }

    EXPECT_THROW((void)AudioConverter::ConvertWavToOgg("/nonexistent/path/audio.wav"), std::runtime_error);
}

TEST_F(AudioConverterTest, ConvertInvalidFileThrows)
{
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available";
    }

    // Create a file that's not actually audio
    auto fakePath = FileUtils::GetTempFilePath(".wav");
    createdFiles_.insert(fakePath);

    std::ofstream file(fakePath);
    file << "This is not a WAV file";
    file.close();

    EXPECT_THROW((void)AudioConverter::ConvertWavToOgg(fakePath), std::runtime_error);
}

TEST_F(AudioConverterTest, RoundtripConversion)
{
    if (!ffmpegAvailable_)
    {
        GTEST_SKIP() << "ffmpeg not available";
    }

    auto originalWav = CreateTestWavFile();
    auto originalSize = std::filesystem::file_size(originalWav);

    // WAV -> OGG -> WAV
    auto oggPath = AudioConverter::ConvertWavToOgg(originalWav);
    createdFiles_.insert(oggPath);

    auto finalWav = AudioConverter::ConvertOggToWav(oggPath);
    createdFiles_.insert(finalWav);

    EXPECT_TRUE(std::filesystem::exists(finalWav));

    // The final WAV should have reasonable size (lossy compression may change it)
    auto finalSize = std::filesystem::file_size(finalWav);
    EXPECT_GT(finalSize, 0u);

    // OGG is compressed, so it should be smaller than WAV
    auto oggSize = std::filesystem::file_size(oggPath);
    EXPECT_LT(oggSize, originalSize);
}

} // namespace mbb::tests

#endif // AUDIOCONVERTERTEST_H
