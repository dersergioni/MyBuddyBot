#include "infra/Env.h"

#include <fmt/format.h>

#include <cstdlib>
#include <stdexcept>

namespace mbb
{

std::string Env::GetRequired(const char* name)
{
    const char* env = std::getenv(name);
    if (env == nullptr)
    {
        throw std::runtime_error(fmt::format("Missing required environment variable: {}", name));
    }
    return env;
}

std::string Env::GetOptional(const char* name)
{
    const char* env = std::getenv(name);
    return env ? std::string(env) : std::string();
}

bool Env::Has(const char* name)
{
    return std::getenv(name) != nullptr;
}

} // namespace mbb
