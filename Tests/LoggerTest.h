#ifndef LOGGERTEST_H
#define LOGGERTEST_H

#include "../core/Config.h"
#include "../core/Logger.h"

#include <gtest/gtest.h>

#include <regex>
#include <sstream>
#include <string>

namespace mbb::tests
{

class LoggerTest : public testing::Test
{
  protected:
    LoggerTest()
    {
        // Disable test mode to allow logging
        Config::SetTestMode(false);
    }

    ~LoggerTest() override
    {
        // Restore test mode
        Config::SetTestMode(true);
    }

    // Helper to capture stderr
    std::string CaptureStderr(std::function<void()> func)
    {
        std::streambuf* oldCerr = std::cerr.rdbuf();
        std::ostringstream captured;
        std::cerr.rdbuf(captured.rdbuf());

        func();

        std::cerr.rdbuf(oldCerr);
        return captured.str();
    }

    // Helper to capture stdout
    std::string CaptureStdout(std::function<void()> func)
    {
        std::streambuf* oldCout = std::cout.rdbuf();
        std::ostringstream captured;
        std::cout.rdbuf(captured.rdbuf());

        func();

        std::cout.rdbuf(oldCout);
        return captured.str();
    }
};

TEST_F(LoggerTest, InfoLogsToStdout)
{
    auto output = CaptureStdout([]() { Logger::Info("Test info message"); });

    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("Test info message") != std::string::npos);
    EXPECT_TRUE(output.find("[I]") != std::string::npos);
}

TEST_F(LoggerTest, ErrorLogsToStderr)
{
    auto output = CaptureStderr([]() { Logger::Error("Test error message"); });

    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("Test error message") != std::string::npos);
    EXPECT_TRUE(output.find("[E]") != std::string::npos);
}

TEST_F(LoggerTest, TimestampFormatIsCorrect)
{
    auto output = CaptureStdout([]() { Logger::Info("timestamp test"); });

    // Timestamp format: YYYY-MM-DD HH:MM:SS[.microseconds]
    // Example: 2026-01-25 19:58:30:[I] message
    std::regex timestampRegex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?:\[I\])");

    EXPECT_TRUE(std::regex_search(output, timestampRegex))
        << "Output doesn't match expected timestamp format: " << output;
}

TEST_F(LoggerTest, InfoMarkedWithI)
{
    auto output = CaptureStdout([]() { Logger::Info("check level"); });
    EXPECT_TRUE(output.find(":[I]") != std::string::npos);
}

TEST_F(LoggerTest, ErrorMarkedWithE)
{
    auto output = CaptureStderr([]() { Logger::Error("check level"); });
    EXPECT_TRUE(output.find(":[E]") != std::string::npos);
}

TEST_F(LoggerTest, MessageEndsWithNewline)
{
    auto output = CaptureStdout([]() { Logger::Info("newline test"); });
    EXPECT_FALSE(output.empty());
    EXPECT_EQ('\n', output.back());
}

TEST_F(LoggerTest, EmptyMessageStillLogs)
{
    auto output = CaptureStdout([]() { Logger::Info(""); });

    // Should still have timestamp and level indicator
    EXPECT_TRUE(output.find(":[I]") != std::string::npos);
}

TEST_F(LoggerTest, SpecialCharactersInMessage)
{
    auto output = CaptureStdout([]() { Logger::Info("Special: \t\r quotes \" and 'single'"); });

    EXPECT_TRUE(output.find("Special:") != std::string::npos);
    EXPECT_TRUE(output.find("quotes") != std::string::npos);
}

TEST_F(LoggerTest, UnicodeInMessage)
{
    auto output = CaptureStdout([]() { Logger::Info("Unicode: Привет 你好 🌍"); });

    EXPECT_TRUE(output.find("Привет") != std::string::npos);
    EXPECT_TRUE(output.find("你好") != std::string::npos);
}

TEST_F(LoggerTest, LongMessage)
{
    std::string longMsg(1000, 'x');
    auto output = CaptureStdout([&longMsg]() { Logger::Info(longMsg); });

    EXPECT_TRUE(output.find(longMsg) != std::string::npos);
}

TEST_F(LoggerTest, TestModeSuppressesInfoAndDebug)
{
    Config::SetTestMode(true);

    auto infoOutput = CaptureStdout([]() { Logger::Info("should not appear"); });

    auto debugOutput = CaptureStdout([]() { Logger::Debug("should not appear"); });

    EXPECT_TRUE(infoOutput.empty());
    EXPECT_TRUE(debugOutput.empty());

    Config::SetTestMode(false);
}

TEST_F(LoggerTest, ErrorNotSuppressedInTestMode)
{
    Config::SetTestMode(true);

    auto errorOutput = CaptureStderr([]() { Logger::Error("error in test mode"); });

    // Error should still be logged even in test mode
    EXPECT_FALSE(errorOutput.empty());
    EXPECT_TRUE(errorOutput.find("error in test mode") != std::string::npos);

    Config::SetTestMode(false);
}

} // namespace mbb::tests

#endif // LOGGERTEST_H
