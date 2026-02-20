#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace mbb
{

class TelegramApi;

class IMessageWorker
{
  public:
    virtual ~IMessageWorker() = default;

    virtual void Start(std::shared_ptr<TelegramApi> api) = 0;
    virtual void Stop() = 0;

    virtual std::optional<uint32_t> AddMessagePortion(std::optional<uint32_t> id,
                                                       int64_t chatId,
                                                       int32_t threadId,
                                                       const std::string& responseText) = 0;

    virtual void FinalizeMessage(std::optional<uint32_t> id) = 0;
};

} // namespace mbb
