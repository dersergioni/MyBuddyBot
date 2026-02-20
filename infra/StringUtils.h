#pragma once

#include <string>

namespace mbb
{

class StringUtils
{
  public:
    [[nodiscard]] static std::string TrimCopy(std::string value);
    [[nodiscard]] static std::string NormalizeUsername(std::string value);
};

} // namespace mbb
