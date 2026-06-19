#include "infra/ResponseSaver.h"

#include "core/Config.h"
#include "core/Logger.h"
#include "infra/FileUtils.h"

#include <fmt/format.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <chrono>
#include <ctime>

namespace mbb
{

std::string ResponseSaver::SaveResponse(const std::string& rawMarkdown)
{
    const auto& viewerDir = Config::GetViewerDir();
    if (viewerDir.empty())
    {
        return "";
    }

    try
    {
        auto id = FileUtils::GenerateUniqueId();

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        gmtime_s(&tm, &timestamp);
#else
        gmtime_r(&timestamp, &tm);
#endif
        auto isoTime = fmt::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}Z", tm.tm_year + 1900, tm.tm_mon + 1,
                                   tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

        rapidjson::Document doc;
        doc.SetObject();
        doc.AddMember("content", rapidjson::Value(rawMarkdown.c_str(), doc.GetAllocator()), doc.GetAllocator());
        doc.AddMember("timestamp", rapidjson::Value(isoTime.c_str(), doc.GetAllocator()), doc.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        auto filePath = viewerDir / "responses" / fmt::format("{}.json", id);
        FileUtils::WriteTextFile(filePath, buffer.GetString());
        return id;
    }
    catch (const std::exception& e)
    {
        Logger::Error(fmt::format("[ResponseSaver] Failed to save: {}", e.what()));
        return "";
    }
}

std::string ResponseSaver::BuildViewerUrl(const std::string& responseId)
{
    return fmt::format("{}?id={}", Config::GetViewerUrl(), responseId);
}

} // namespace mbb
