#include "telegram/MessageWorker.h"

#include "core/Config.h"
#include "core/Logger.h"
#include "infra/ResponseSaver.h"
#include "infra/StringUtils.h"
#include "telegram/MessageSplitter.h"
#include "telegram/TelegramApi.h"

#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <ranges>

namespace mbb
{

constexpr int kWorkerLoopIntervalMs = 3000;      // Worker processes blocks every 3 seconds
constexpr int kSendFailureBackoffMultiplier = 2; // Timeout multiplier on failed message send
constexpr int kEditFailureBackoffMultiplier = 3; // Timeout multiplier on failed message edit
constexpr auto kMinBlockUpdateInterval = std::chrono::milliseconds(1100);
constexpr auto kBlockUpdateWindow = std::chrono::milliseconds(66000);
constexpr size_t kMaxBlockUpdatesPerWindow = 20;

const std::chrono::seconds MessageWorker::kUpdateTimeout = std::chrono::seconds(1);

namespace
{

void PruneExpiredBlockRateLimitTimestamps(MessageBlock& block, const std::chrono::system_clock::time_point now)
{
    while (!block.recentUpdateTimes.empty() && now - block.recentUpdateTimes.front() >= kBlockUpdateWindow)
    {
        block.recentUpdateTimes.pop_front();
    }
}

bool IsBlockUpdateAllowedByRateLimits(MessageBlock& block, const std::chrono::system_clock::time_point now)
{
    PruneExpiredBlockRateLimitTimestamps(block, now);

    if (now - block.lastUpdateTime < kMinBlockUpdateInterval)
    {
        return false;
    }

    return block.recentUpdateTimes.size() < kMaxBlockUpdatesPerWindow;
}

void RecordBlockUpdateForRateLimits(MessageBlock& block, const std::chrono::system_clock::time_point now)
{
    block.lastUpdateTime = now;
    block.recentUpdateTimes.push_back(now);
}

} // namespace

MessageWorker::~MessageWorker()
{
    Stop();
}

void MessageWorker::Start(std::shared_ptr<TelegramApi> api)
{
    stopping = false;
    api_ = api;
    workerThread_ = std::thread(&MessageWorker::WorkerLoop, this);
}

void MessageWorker::Stop()
{
    stopping = true;
    cv_.notify_all();
    if (workerThread_.joinable())
    {
        workerThread_.join();
        Logger::Debug("MessageWorker stopped");
    }
}

void MessageWorker::WorkerLoop()
{
    while (!stopping)
    {
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(kWorkerLoopIntervalMs),
                     [this] { return (!messageBlocks_.empty() && newPortion_) || stopping; });

        for (auto& block : messageBlocks_)
        {
            ProcessMessageBlock(block);
        }

        // Remove completed blocks
        auto it = std::ranges::remove_if(messageBlocks_, [](const MessageBlock& block) {
                      return std::ranges::all_of(block.subBlocks,
                                                 [](const TgMessageSubBlock& sub) { return sub.isDelivered; });
                  }).begin();
        messageBlocks_.erase(it, messageBlocks_.end());
        newPortion_ = false;
    }
}

void MessageWorker::ProcessMessageBlock(MessageBlock& block)
{
    auto api = api_.lock();
    if (!api)
    {
        return;
    }

    MessageSplitter::SplitForStreaming(block);
    if (block.isReadyToFinalize)
    {
        MessageSplitter::FinalizeSubBlocks(block);
    }

    for (auto& subBlock : block.subBlocks)
    {
        if (subBlock.isDelivered)
        {
            continue;
        }

        auto now = std::chrono::system_clock::now();
        if (!IsBlockUpdateAllowedByRateLimits(block, now))
        {
            continue;
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(now - subBlock.lastSentTime) <
            kUpdateTimeout * subBlock.factorTimeout)
        {
            continue;
        }

        if (subBlock.lastSentText == subBlock.tgText)
        {
            continue;
        }

        TgBot::Message::Ptr result;
        RecordBlockUpdateForRateLimits(block, now);
        if (subBlock.message == nullptr)
        {
            // Send new message
            result = api->SendMessage(block.chatId, block.threadId, subBlock.tgText, subBlock.parseMode);
            subBlock.message = result;

            if (!result)
            {
                subBlock.factorTimeout *= kSendFailureBackoffMultiplier;
                Logger::Debug(fmt::format("[MODE:'{}';BLOCK:{}]Can not send:\n[BEGIN]{}[END]\n\n", subBlock.parseMode,
                                          subBlock.number, subBlock.tgText));
            }
        }
        else
        {
            // Edit existing message
            result = api->EditMessage(block.chatId, subBlock.message->messageId, subBlock.tgText, subBlock.parseMode);

            if (!result)
            {
                subBlock.factorTimeout *= kEditFailureBackoffMultiplier;
                Logger::Debug(fmt::format("[MODE:'{}';BLOCK:{}]Can not edit:\n", subBlock.parseMode, subBlock.number));
                Logger::Debug(fmt::format(
                    "Original [BEGIN]\n{}\n[END]\n\n",
                    block.rawFullText.substr(subBlock.range.first, subBlock.range.second - subBlock.range.first)));
                Logger::Debug(fmt::format("Edited [BEGIN]\n{}\n[END]\n\n", subBlock.tgText));
            }
        }

        if (result)
        {
            subBlock.lastSentTime = now;
            subBlock.lastSentText = subBlock.tgText;
        }

        if (subBlock.isFinalized && (subBlock.finalizedAttempts-- <= 0 || result))
        {
            subBlock.isDelivered = true;
        }
    }

    SendViewerButtonIfNeeded(block, api);
}

void MessageWorker::SendViewerButtonIfNeeded(MessageBlock& block, const std::shared_ptr<TelegramApi>& api)
{
    if (block.viewerButtonSent)
    {
        return;
    }

    bool hasUrl = !Config::GetViewerUrl().empty();
    bool hasDir = !Config::GetViewerDir().empty();
    bool allDone = std::ranges::all_of(block.subBlocks, [](const TgMessageSubBlock& sub) { return sub.isDelivered; });
    bool hasLatex = allDone && StringUtils::ContainsLatex(block.rawFullText);

    if (hasUrl && hasDir && allDone && hasLatex)
    {
        auto responseId = ResponseSaver::SaveResponse(block.rawFullText);
        if (!responseId.empty())
        {
            auto viewerUrl = ResponseSaver::BuildViewerUrl(responseId);
            api->SendMessageWithUrlButton(block.chatId, block.threadId, "View full response:", "Open in viewer",
                                          viewerUrl);
        }
        else
        {
            Logger::Error("[Viewer] ResponseSaver::SaveResponse returned empty ID");
        }
        block.viewerButtonSent = true;
    }
}

std::optional<uint32_t> MessageWorker::AddMessagePortion(const std::optional<uint32_t> id,
                                                         const int64_t chatId,
                                                         const int32_t threadId,
                                                         const std::string& responseText)
{
    std::optional<uint32_t> workerId = std::nullopt;

    if (id.has_value())
    {
        std::lock_guard lock(mutex_);
        for (auto& block : messageBlocks_)
        {
            if (block.id == id.value())
            {
                block.rawFullText = responseText;
                workerId = block.id;
                break;
            }
        }
    }
    else
    {
        MessageBlock block;
        block.id = idSequence_++;
        block.chatId = chatId;
        block.threadId = threadId;
        block.rawFullText = responseText;
        block.subBlocks.emplace_back();

        workerId = block.id;
        std::lock_guard lock(mutex_);
        messageBlocks_.push_back(std::move(block));
    }

    newPortion_ = true;
    cv_.notify_one();
    return workerId;
}

void MessageWorker::FinalizeMessage(std::optional<uint32_t> id)
{
    if (id.has_value())
    {
        std::lock_guard lock(mutex_);
        for (auto& block : messageBlocks_)
        {
            if (block.id == id.value())
            {
                block.isReadyToFinalize = true;
                newPortion_ = true;
                cv_.notify_one();
            }
        }
    }
}

} // namespace mbb
