#pragma once

#include <string>

namespace mbb
{

class Env
{
  public:
    [[nodiscard]] static std::string GetRequired(const char* name);
    [[nodiscard]] static std::string GetOptional(const char* name);
    [[nodiscard]] static bool Has(const char* name);
};

} // namespace mbb
