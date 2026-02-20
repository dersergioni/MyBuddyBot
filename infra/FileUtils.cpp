#include "infra/FileUtils.h"

#include <fmt/core.h>

#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>

namespace mbb
{

std::filesystem::path FileUtils::GetTempFilePath(const std::string& extension)
{
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<int> dist(10000, 99999);

    std::filesystem::path path;
    do
    {
        auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        auto random = dist(rng);
        auto filename = fmt::format("tmp_{}_{}{}", timestamp, random, extension);
        path = std::filesystem::temp_directory_path() / filename;
    } while (std::filesystem::exists(path));

    return path;
}

std::vector<char> FileUtils::ReadBinaryFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error(fmt::format("Failed to open file for reading: {}", path.string()));
    }

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(fileSize));
    file.read(buffer.data(), fileSize);

    return buffer;
}

void FileUtils::WriteBinaryFile(const std::filesystem::path& path, const std::vector<char>& data)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(fmt::format("Failed to open file for writing: {}", path.string()));
    }

    file.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void FileUtils::WriteBinaryFile(const std::filesystem::path& path, const std::string& data)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(fmt::format("Failed to open file for writing: {}", path.string()));
    }

    file.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void FileUtils::RemoveFile(const std::filesystem::path& path)
{
    std::filesystem::remove(path);
}

ScopedTempFile::ScopedTempFile(std::filesystem::path path) : path_(std::move(path))
{
}

ScopedTempFile::~ScopedTempFile()
{
    if (!path_.empty())
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
}

ScopedTempFile::ScopedTempFile(ScopedTempFile&& other) noexcept : path_(std::move(other.path_))
{
    other.path_.clear();
}

ScopedTempFile& ScopedTempFile::operator=(ScopedTempFile&& other) noexcept
{
    if (this != &other)
    {
        if (!path_.empty())
        {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
        path_ = std::move(other.path_);
        other.path_.clear();
    }
    return *this;
}

const std::filesystem::path& ScopedTempFile::Path() const
{
    return path_;
}

void ScopedTempFile::Release()
{
    path_.clear();
}

} // namespace mbb
