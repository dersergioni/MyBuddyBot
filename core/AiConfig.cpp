#include "core/AiConfig.h"

#include "core/Logger.h"
#include "infra/StringUtils.h"

#include <fmt/format.h>
#include <fmt/std.h>
#include <rapidjson/document.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mbb
{

namespace
{
AiModelSpec ParseModelSpec(const rapidjson::Value& model)
{
    AiModelSpec result;
    if (model.HasMember("name") && model["name"].IsString())
    {
        result.name = model["name"].GetString();
    }
    if (model.HasMember("url") && model["url"].IsString())
    {
        result.url = model["url"].GetString();
    }
    if (model.HasMember("contextSize") && model["contextSize"].IsUint64())
    {
        result.contextSize = static_cast<std::size_t>(model["contextSize"].GetUint64());
    }
    return result;
}
} // namespace

void AiConfig::LoadFromFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error(fmt::format("Failed to open AI config file: {}", path));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    LoadFromString(buffer.str());

    Logger::Info(fmt::format("AI config loaded from {}", path));
}

void AiConfig::LoadFromString(const std::string& json)
{
    providers_.clear();

    // Skip a leading UTF-8 BOM, which some editors prepend and which RapidJSON
    // would otherwise reject as malformed.
    const char* content = json.c_str();
    if (json.size() >= 3 && static_cast<unsigned char>(json[0]) == 0xEF &&
        static_cast<unsigned char>(json[1]) == 0xBB && static_cast<unsigned char>(json[2]) == 0xBF)
    {
        content += 3;
    }

    rapidjson::Document doc;
    doc.Parse(content);

    if (doc.HasParseError())
    {
        throw std::runtime_error("AI config contains malformed JSON");
    }
    if (!doc.IsObject() || !doc.HasMember("providers") || !doc["providers"].IsObject())
    {
        throw std::runtime_error("AI config must be an object with a \"providers\" object");
    }

    for (const auto& providerNode : doc["providers"].GetObject())
    {
        if (!providerNode.value.IsObject())
        {
            continue;
        }

        const auto& provider = providerNode.value;
        ProviderEntry entry;

        if (provider.HasMember("apiKey") && provider["apiKey"].IsString())
        {
            entry.apiKey = provider["apiKey"].GetString();
        }

        if (provider.HasMember("models") && provider["models"].IsObject())
        {
            for (const auto& modelNode : provider["models"].GetObject())
            {
                if (modelNode.value.IsObject())
                {
                    entry.models[StringUtils::ToLower(modelNode.name.GetString())] = ParseModelSpec(modelNode.value);
                }
            }
        }

        providers_[StringUtils::ToLower(providerNode.name.GetString())] = std::move(entry);
    }
}

std::optional<std::string> AiConfig::GetApiKey(const std::string& provider) const
{
    const auto it = providers_.find(provider);
    if (it == providers_.end())
    {
        return std::nullopt;
    }
    return it->second.apiKey;
}

AiModelSpec AiConfig::GetModelSpec(const std::string& provider, const std::string& slot) const
{
    const auto providerIt = providers_.find(provider);
    if (providerIt == providers_.end())
    {
        return {};
    }

    const auto modelIt = providerIt->second.models.find(slot);
    if (modelIt == providerIt->second.models.end())
    {
        return {};
    }

    return modelIt->second;
}

} // namespace mbb
