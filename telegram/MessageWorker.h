#pragma once

#include "telegram/IMessageWorker.h"

#include <tgbot/tgbot.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace mbb
{

class TelegramApi;

// =============================================================================
// Message streaming structures
// =============================================================================

struct TgMessageSubBlock
{
    uint32_t number = 1;
    std::string tgOutcomingText;
    std::string unformattedText;
    std::string parseMode;
    std::pair<size_t, size_t> range = {0, 0};
    TgBot::Message::Ptr message = nullptr;
    std::string tgLastSentText;
    std::chrono::time_point<std::chrono::system_clock> lastSent = {};
    static constexpr int32_t kMaxFinalizeAttempts = 3; // Retry finalized send/edit before giving up
    int factorTimeout = 1;
    bool isFilled = false;
    bool isReadyToDelete = false;
    int32_t finalizedAttempts = kMaxFinalizeAttempts;
};

struct MessageBlock
{
    uint32_t id;
    int64_t chatId;
    int32_t threadId;
    bool isReadyToFinalize;
    bool viewerButtonSent = false;
    std::string incomingText;
    std::vector<TgMessageSubBlock> subBlocks;
};

// =============================================================================
// MessageWorker - Handles streaming message updates to Telegram
// =============================================================================

class MessageWorker final : public IMessageWorker
{
  public:
    MessageWorker() = default;
    ~MessageWorker() override;

    // Lifecycle
    void Start(std::shared_ptr<TelegramApi> api) override;
    void Stop() override;

    // Message streaming
    std::optional<uint32_t> AddMessagePortion(std::optional<uint32_t> id,
                                              int64_t chatId,
                                              int32_t threadId,
                                              const std::string& responseText) override;

    void FinalizeMessage(std::optional<uint32_t> id) override;

    std::atomic<bool> stopping{false};

  private:
    void WorkerLoop();
    void ProcessMessageBlock(MessageBlock& block);

    static void BalanceSubBlocks(MessageBlock& block);
    static void FinalizeSubBlocks(MessageBlock& block);
    static std::string ConvertMarkdownToTelegramHtml(const std::string& input);
    static std::string Trim(const std::string& input);

#ifdef BUILD_TESTS
  public:
    static std::vector<std::string> FormatForTest(const std::string& input);
    static size_t GetMaxMessageLengthForTest();
#endif

    static constexpr size_t kMaxMessageLength = 4096;
    static constexpr size_t kSplitThreshold = 3686;
    static const std::chrono::seconds kUpdateTimeout;
    static const std::string kPartMessagePrefix;

    std::weak_ptr<TelegramApi> api_;
    std::vector<MessageBlock> messageBlocks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    uint32_t idSequence_ = 0;
    std::thread workerThread_;
    std::atomic<bool> newPortion_{false};
};

} // namespace mbb
