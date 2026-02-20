#include "infra/StringUtils.h"

#include <algorithm>
#include <cctype>

namespace mbb
{

std::string StringUtils::TrimCopy(std::string value)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string StringUtils::NormalizeUsername(std::string value)
{
    std::string normalized = TrimCopy(std::move(value));
    if (!normalized.empty() && normalized.front() == '@')
    {
        normalized.erase(normalized.begin());
    }
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

} // namespace mbb
