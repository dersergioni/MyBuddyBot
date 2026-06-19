#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace mbb
{

struct AiModelSpec
{
    std::optional<std::string> name;
    std::optional<std::string> url;
    std::optional<std::size_t> contextSize;

    [[nodiscard]] bool IsComplete() const
    {
        return name.has_value() && url.has_value() && contextSize.has_value();
    }
};

class AiConfig
{
  public:
    void LoadFromFile(const std::filesystem::path& path);

    void LoadFromString(const std::string& json);

    [[nodiscard]] std::optional<std::string> GetApiKey(const std::string& provider) const;

    [[nodiscard]] AiModelSpec GetModelSpec(const std::string& provider, const std::string& slot) const;

  private:
    struct ProviderEntry
    {
        std::optional<std::string> apiKey;
        std::map<std::string, AiModelSpec> models;
    };

    std::map<std::string, ProviderEntry> providers_;
};

} // namespace mbb
