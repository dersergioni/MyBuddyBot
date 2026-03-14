#pragma once

#include "telegram/IMessageWorker.h"
#include "telegram/MessageTypes.h"

#include <atomic>
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
    void SendViewerButtonIfNeeded(MessageBlock& block, const std::shared_ptr<TelegramApi>& api);

    static const std::chrono::seconds kUpdateTimeout;

    std::weak_ptr<TelegramApi> api_;
    std::vector<MessageBlock> messageBlocks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    uint32_t idSequence_ = 0;
    std::thread workerThread_;
    std::atomic<bool> newPortion_{false};
};

} // namespace mbb
