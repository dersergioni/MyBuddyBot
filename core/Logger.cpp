#include "core/Logger.h"

#include "core/Config.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <iostream>
#include <mutex>

namespace mbb
{

namespace
{
std::mutex logMutex;
}

std::string Logger::FormatMessage(const std::string& msg, Level level)
{
    const auto now = std::chrono::system_clock::now();

    char levelChar = 'I';
    switch (level)
    {
    case Level::Info:
        levelChar = 'I';
        break;
    case Level::Debug:
        levelChar = 'D';
        break;
    case Level::Error:
        levelChar = 'E';
        break;
    }

    return fmt::format("{:%Y-%m-%d %H:%M:%S}:[{}] {}\n", now, levelChar, msg);
}

void Logger::Info(const std::string& msg)
{
    if (Config::IsTestMode())
        return;

    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << FormatMessage(msg, Level::Info);
}

void Logger::Debug(const std::string& msg)
{
    if (Config::IsTestMode())
        return;

    if (!Config::IsDebugMode())
        return;

    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << FormatMessage(msg, Level::Debug);
}

void Logger::Error(const std::string& msg)
{
    constexpr const char* RED = "\033[31m";
    constexpr const char* RESET = "\033[0m";

    const auto messageToLog = FormatMessage(msg, Level::Error);

    std::lock_guard<std::mutex> lock(logMutex);
#ifndef _WIN32
    std::cerr << RED << messageToLog << RESET;
#else
    std::cerr << messageToLog;
#endif
}

} // namespace mbb
