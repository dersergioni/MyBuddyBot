#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mbb
{

class FileUtils
{
  public:
    // Generate a unique temporary file path with given extension
    [[nodiscard]] static std::filesystem::path GetTempFilePath(const std::string& extension);

    // Read entire file as binary
    [[nodiscard]] static std::vector<char> ReadBinaryFile(const std::filesystem::path& path);

    // Write binary data to file
    static void WriteBinaryFile(const std::filesystem::path& path, const std::vector<char>& data);

    // Write string data to file
    static void WriteBinaryFile(const std::filesystem::path& path, const std::string& data);

    // Write text file, creating parent directories if needed
    static void WriteTextFile(const std::filesystem::path& path, const std::string& content);

    // Generate a unique ID string (hex timestamp + random suffix)
    [[nodiscard]] static std::string GenerateUniqueId();

    // Remove file (throws std::filesystem::filesystem_error on failure)
    static void RemoveFile(const std::filesystem::path& path);
};

class ScopedTempFile
{
  public:
    explicit ScopedTempFile(std::filesystem::path path);
    ~ScopedTempFile();

    ScopedTempFile(const ScopedTempFile&) = delete;
    ScopedTempFile& operator=(const ScopedTempFile&) = delete;

    ScopedTempFile(ScopedTempFile&& other) noexcept;
    ScopedTempFile& operator=(ScopedTempFile&& other) noexcept;

    [[nodiscard]] const std::filesystem::path& Path() const;
    void Release();

  private:
    std::filesystem::path path_;
};

} // namespace mbb
