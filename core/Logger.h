#pragma once

#include <string>

namespace mbb
{

class Logger
{
  public:
    static void Info(const std::string& msg);
    static void Debug(const std::string& msg);
    static void Error(const std::string& msg);

  private:
    enum class Level
    {
        Info,
        Debug,
        Error
    };
    static std::string FormatMessage(const std::string& msg, Level level);
};

} // namespace mbb
