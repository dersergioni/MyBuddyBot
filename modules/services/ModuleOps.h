#pragma once

#include "rust/cxx.h"

#include <cstdint>
#include <memory>
#include <string>

namespace mbb
{

class TelegramApi;
class TelegramOps;
class Storage;

struct CxxInlineButton;

// =============================================================================
// ModuleOps — host operations available to Rust modules via CXX bridge
// =============================================================================

class ModuleOps
{
  public:
    ModuleOps(std::shared_ptr<TelegramOps> ops, std::shared_ptr<Storage> storage);

    // TelegramOps wrappers with CXX-compatible types.
    // snake_case because these are called from Rust via CXX bridge.
    void send_text(int64_t chat_id, int32_t thread_id, rust::Str text) const;
    int32_t send_with_inline_buttons(int64_t chat_id,
                                     int32_t thread_id,
                                     rust::Str text,
                                     rust::Slice<const CxxInlineButton> buttons,
                                     rust::Slice<const int32_t> row_sizes) const;
    void edit_text(int64_t chat_id, int32_t message_id, rust::Str text) const;
    void edit_with_inline_buttons(int64_t chat_id,
                                  int32_t message_id,
                                  rust::Str text,
                                  rust::Slice<const CxxInlineButton> buttons,
                                  rust::Slice<const int32_t> row_sizes) const;
    void answer_callback(rust::Str callback_query_id, rust::Str text) const;
    void delete_message(int64_t chat_id, int32_t message_id) const;
    void send_text_and_remove_reply_keyboard(int64_t chat_id, int32_t thread_id, rust::Str text) const;

    // User registry lookups (delegates to Storage)
    int64_t lookup_user_by_username(rust::Str username) const;
    rust::String get_user_display_name(int64_t user_id) const;

  private:
    std::shared_ptr<TelegramOps> ops_;
    std::shared_ptr<Storage> storage_;
};

// Logger wrappers for Rust → C++ logging
void log_info(rust::Str msg);
void log_debug(rust::Str msg);

} // namespace mbb
