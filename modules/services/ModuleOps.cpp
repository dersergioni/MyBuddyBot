#include "modules/services/ModuleOps.h"

#include "modules/services/TelegramOps.h"
#include "wishlist_cxx/lib.h"

#include "core/Logger.h"
#include "core/Storage.h"

namespace mbb
{

namespace
{
std::vector<InlineButtonRow> ConvertButtons(rust::Slice<const CxxInlineButton> buttons,
                                            rust::Slice<const int32_t> row_sizes)
{
    std::vector<InlineButtonRow> rows;
    size_t idx = 0;
    for (auto size : row_sizes)
    {
        InlineButtonRow row;
        for (int32_t i = 0; i < size && idx < buttons.size(); ++i, ++idx)
        {
            row.push_back({std::string(buttons[idx].label), std::string(buttons[idx].callback_data)});
        }
        rows.push_back(std::move(row));
    }
    return rows;
}
} // namespace

// =============================================================================
// ModuleOps
// =============================================================================

ModuleOps::ModuleOps(std::shared_ptr<TelegramOps> ops, std::shared_ptr<Storage> storage)
    : ops_(std::move(ops)), storage_(std::move(storage))
{
}

void ModuleOps::send_text(int64_t chat_id, int32_t thread_id, rust::Str text) const
{
    ops_->SendText(chat_id, thread_id, std::string(text));
}

int32_t ModuleOps::send_with_inline_buttons(int64_t chat_id,
                                            int32_t thread_id,
                                            rust::Str text,
                                            rust::Slice<const CxxInlineButton> buttons,
                                            rust::Slice<const int32_t> row_sizes) const
{
    return ops_->SendWithInlineButtons(chat_id, thread_id, std::string(text), ConvertButtons(buttons, row_sizes));
}

void ModuleOps::edit_text(int64_t chat_id, int32_t message_id, rust::Str text) const
{
    ops_->EditText(chat_id, message_id, std::string(text));
}

void ModuleOps::edit_with_inline_buttons(int64_t chat_id,
                                         int32_t message_id,
                                         rust::Str text,
                                         rust::Slice<const CxxInlineButton> buttons,
                                         rust::Slice<const int32_t> row_sizes) const
{
    ops_->EditWithInlineButtons(chat_id, message_id, std::string(text), ConvertButtons(buttons, row_sizes));
}

void ModuleOps::answer_callback(rust::Str callback_query_id, rust::Str text) const
{
    ops_->AnswerCallback(std::string(callback_query_id), std::string(text));
}

void ModuleOps::delete_message(int64_t chat_id, int32_t message_id) const
{
    ops_->DeleteMessage(chat_id, message_id);
}

void ModuleOps::send_text_and_remove_reply_keyboard(int64_t chat_id, int32_t thread_id, rust::Str text) const
{
    ops_->SendTextAndRemoveReplyKeyboard(chat_id, thread_id, std::string(text));
}

int64_t ModuleOps::lookup_user_by_username(rust::Str username) const
{
    return storage_->LookupUserByUsername(std::string(username));
}

rust::String ModuleOps::get_user_display_name(int64_t user_id) const
{
    return rust::String(storage_->GetUserDisplayName(user_id));
}

// =============================================================================
// Logger wrappers
// =============================================================================

void log_info(rust::Str msg)
{
    Logger::Info(std::string(msg));
}

void log_debug(rust::Str msg)
{
    Logger::Debug(std::string(msg));
}

} // namespace mbb
