#pragma once

#include <tgbot/tgbot.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace mbb
{

// =============================================================================
// Message streaming structures
// =============================================================================

struct TgMessageSubBlock
{
    uint32_t number = 1;
    bool isFinalized = false;
    bool isDelivered = false;
    std::string tgText;
    std::string markdownPrefix;
    std::string markdownSuffix;
    std::string parseMode = "HTML";
    std::pair<size_t, size_t> range;
    TgBot::Message::Ptr message;
    std::string lastSentText;
    std::chrono::time_point<std::chrono::system_clock> lastSentTime;
    int factorTimeout = 1;
    static constexpr int32_t kMaxFinalizeAttempts = 3;
    int32_t finalizedAttempts = kMaxFinalizeAttempts;
};

struct MessageBlock
{
    uint32_t id = 0;
    int64_t chatId = 0;
    int32_t threadId = 0;
    bool isReadyToFinalize = false;
    bool viewerButtonSent = false;
    std::string rawFullText;
    std::vector<TgMessageSubBlock> subBlocks;
    std::chrono::time_point<std::chrono::system_clock> lastUpdateTime;
    std::deque<std::chrono::time_point<std::chrono::system_clock>> recentUpdateTimes;
};

} // namespace mbb
