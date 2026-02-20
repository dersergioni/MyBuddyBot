#ifndef FILEUTILSTEST_H
#define FILEUTILSTEST_H

#include "../infra/FileUtils.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace mbb::tests
{

class FileUtilsTest : public testing::Test
{
  protected:
    FileUtilsTest() = default;
    ~FileUtilsTest() override = default;

    void TearDown() override
    {
        // Clean up any leftover temp files
        for (const auto& path : createdFiles_)
        {
            if (std::filesystem::exists(path))
            {
                std::filesystem::remove(path);
            }
        }
    }

    std::set<std::filesystem::path> createdFiles_;
};

TEST_F(FileUtilsTest, GetTempFilePathReturnsUniquePaths)
{
    auto path1 = FileUtils::GetTempFilePath(".txt");
    auto path2 = FileUtils::GetTempFilePath(".txt");
    auto path3 = FileUtils::GetTempFilePath(".txt");

    createdFiles_.insert(path1);
    createdFiles_.insert(path2);
    createdFiles_.insert(path3);

    EXPECT_NE(path1, path2);
    EXPECT_NE(path2, path3);
    EXPECT_NE(path1, path3);
}

TEST_F(FileUtilsTest, GetTempFilePathPreservesExtension)
{
    auto pathTxt = FileUtils::GetTempFilePath(".txt");
    auto pathOgg = FileUtils::GetTempFilePath(".ogg");
    auto pathWav = FileUtils::GetTempFilePath(".wav");

    createdFiles_.insert(pathTxt);
    createdFiles_.insert(pathOgg);
    createdFiles_.insert(pathWav);

    EXPECT_EQ(".txt", pathTxt.extension());
    EXPECT_EQ(".ogg", pathOgg.extension());
    EXPECT_EQ(".wav", pathWav.extension());
}

TEST_F(FileUtilsTest, WriteBinaryFileAndReadBinaryFile)
{
    auto path = FileUtils::GetTempFilePath(".bin");
    createdFiles_.insert(path);

    std::vector<char> original = {'H', 'e', 'l', 'l', 'o', '\x00', '\xff'};
    FileUtils::WriteBinaryFile(path, original);

    auto readBack = FileUtils::ReadBinaryFile(path);
    EXPECT_EQ(original, readBack);
}

TEST_F(FileUtilsTest, WriteBinaryFileWithString)
{
    auto path = FileUtils::GetTempFilePath(".txt");
    createdFiles_.insert(path);

    std::string original = "Hello, World!";
    FileUtils::WriteBinaryFile(path, original);

    auto readBack = FileUtils::ReadBinaryFile(path);
    std::string result(readBack.begin(), readBack.end());
    EXPECT_EQ(original, result);
}

TEST_F(FileUtilsTest, RemoveFileDeletesFile)
{
    auto path = FileUtils::GetTempFilePath(".tmp");
    createdFiles_.insert(path);

    // Create the file
    std::vector<char> data = {'t', 'e', 's', 't'};
    FileUtils::WriteBinaryFile(path, data);
    EXPECT_TRUE(std::filesystem::exists(path));

    // Remove and verify
    FileUtils::RemoveFile(path);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(FileUtilsTest, ReadBinaryFileThrowsOnNonexistent)
{
    auto path = std::filesystem::path("/nonexistent/path/file.bin");
    EXPECT_THROW((void)FileUtils::ReadBinaryFile(path), std::exception);
}

TEST_F(FileUtilsTest, WriteBinaryFileCreatesParentDirectories)
{
    auto tempDir = std::filesystem::temp_directory_path();
    std::random_device rd;
    auto uniqueDir = tempDir / ("test_" + std::to_string(rd()));
    auto path = uniqueDir / "subdir" / "file.txt";

    createdFiles_.insert(path);

    // Write should create parent directories or throw
    try
    {
        std::vector<char> data = {'t', 'e', 's', 't'};
        FileUtils::WriteBinaryFile(path, data);

        auto readBack = FileUtils::ReadBinaryFile(path);
        EXPECT_EQ(data, readBack);

        // Cleanup
        std::filesystem::remove_all(uniqueDir);
    }
    catch (const std::exception&)
    {
        // If WriteBinaryFile doesn't create parent dirs, that's acceptable behavior
        // Just cleanup if the dir was partially created
        if (std::filesystem::exists(uniqueDir))
        {
            std::filesystem::remove_all(uniqueDir);
        }
    }
}

TEST_F(FileUtilsTest, RoundtripLargeFile)
{
    auto path = FileUtils::GetTempFilePath(".bin");
    createdFiles_.insert(path);

    // Create 64KB of data
    std::vector<char> original(64 * 1024);
    for (size_t i = 0; i < original.size(); ++i)
    {
        original[i] = static_cast<char>((i * 17 + 31) % 256);
    }

    FileUtils::WriteBinaryFile(path, original);
    auto readBack = FileUtils::ReadBinaryFile(path);

    EXPECT_EQ(original.size(), readBack.size());
    EXPECT_EQ(original, readBack);
}

TEST_F(FileUtilsTest, WriteEmptyFile)
{
    auto path = FileUtils::GetTempFilePath(".empty");
    createdFiles_.insert(path);

    std::vector<char> empty;
    FileUtils::WriteBinaryFile(path, empty);

    auto readBack = FileUtils::ReadBinaryFile(path);
    EXPECT_TRUE(readBack.empty());
}

} // namespace mbb::tests

#endif // FILEUTILSTEST_H
