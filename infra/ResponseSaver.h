#pragma once

#include <string>

namespace mbb
{

class ResponseSaver
{
  public:
    // Save raw markdown as a JSON file, returns response ID (or empty on failure)
    [[nodiscard]] static std::string SaveResponse(const std::string& rawMarkdown);

    // Build the full viewer URL for a given response ID
    [[nodiscard]] static std::string BuildViewerUrl(const std::string& responseId);
};

} // namespace mbb
