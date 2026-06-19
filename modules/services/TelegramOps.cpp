#include "modules/services/TelegramOps.h"

#include "telegram/TelegramApi.h"

#include <tgbot/tgbot.h>

namespace mbb
{

TelegramOps::TelegramOps(std::shared_ptr<TelegramApi> api) : api_(std::move(api))
{
}

namespace
{
TgBot::InlineKeyboardMarkup::Ptr BuildInlineKeyboard(const std::vector<InlineButtonRow>& rows)
{
    auto keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    for (const auto& row : rows)
    {
        std::vector<TgBot::InlineKeyboardButton::Ptr> tgRow;
        for (const auto& btn : row)
        {
            auto button = std::make_shared<TgBot::InlineKeyboardButton>();
            button->text = btn.label;
            button->callbackData = btn.callbackData;
            tgRow.push_back(button);
        }
        keyboard->inlineKeyboard.push_back(tgRow);
    }
    return keyboard;
}
} // anonymous namespace

void TelegramOps::SendText(int64_t chatId, int32_t threadId, const std::string& text)
{
    api_->SendMessage(chatId, threadId, text);
}

int32_t TelegramOps::SendWithInlineButtons(int64_t chatId,
                                           int32_t threadId,
                                           const std::string& text,
                                           const std::vector<InlineButtonRow>& rows)
{
    auto msg = api_->SendMessageWithInlineKeyboard(chatId, threadId, text, BuildInlineKeyboard(rows));
    return msg ? msg->messageId : 0;
}

void TelegramOps::SendTextAndRemoveReplyKeyboard(int64_t chatId, int32_t threadId, const std::string& text)
{
    api_->SendMessageAndRemoveReplyKeyboard(chatId, threadId, text);
}

void TelegramOps::EditText(int64_t chatId, int32_t messageId, const std::string& text)
{
    api_->EditMessage(chatId, messageId, text);
}

void TelegramOps::EditWithInlineButtons(int64_t chatId,
                                        int32_t messageId,
                                        const std::string& text,
                                        const std::vector<InlineButtonRow>& rows)
{
    api_->EditMessageWithInlineKeyboard(chatId, messageId, text, BuildInlineKeyboard(rows));
}

void TelegramOps::AnswerCallback(const std::string& callbackQueryId, const std::string& text)
{
    api_->AnswerCallbackQuery(callbackQueryId, text);
}

void TelegramOps::DeleteMessage(int64_t chatId, int32_t messageId)
{
    api_->DeleteMessage(chatId, messageId);
}

} // namespace mbb
