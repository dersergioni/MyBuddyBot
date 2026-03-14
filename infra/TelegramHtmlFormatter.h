#pragma once

#include <string>

namespace mbb
{

class TelegramHtmlFormatter
{
  public:
    [[nodiscard]] static std::string ConvertMarkdownToTelegramHtml(const std::string& input);
};

} // namespace mbb
