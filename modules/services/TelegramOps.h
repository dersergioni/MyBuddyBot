#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mbb
{

class TelegramApi;

struct InlineButton
{
    std::string label;
    std::string callbackData;
};

using InlineButtonRow = std::vector<InlineButton>;

class TelegramOps
{
  public:
    explicit TelegramOps(std::shared_ptr<TelegramApi> api);

    // Non-copyable
    TelegramOps(const TelegramOps&) = delete;
    TelegramOps& operator=(const TelegramOps&) = delete;

    void SendText(int64_t chatId, int32_t threadId, const std::string& text);
    [[nodiscard]] int32_t SendWithInlineButtons(int64_t chatId,
                                                int32_t threadId,
                                                const std::string& text,
                                                const std::vector<InlineButtonRow>& rows);
    void SendTextAndRemoveReplyKeyboard(int64_t chatId, int32_t threadId, const std::string& text);

    void EditText(int64_t chatId, int32_t messageId, const std::string& text);
    void EditWithInlineButtons(int64_t chatId,
                               int32_t messageId,
                               const std::string& text,
                               const std::vector<InlineButtonRow>& rows);

    void AnswerCallback(const std::string& callbackQueryId, const std::string& text = "");
    void DeleteMessage(int64_t chatId, int32_t messageId);

  private:
    std::shared_ptr<TelegramApi> api_;
};

} // namespace mbb
