#pragma once

#include <string>

namespace mbb
{

class StringUtils
{
  public:
    [[nodiscard]] static std::string TrimCopy(std::string value);
    [[nodiscard]] static std::string ToLower(std::string value);
    [[nodiscard]] static std::string NormalizeUsername(std::string value);
    [[nodiscard]] static bool ContainsLatex(const std::string& text);
};

} // namespace mbb
