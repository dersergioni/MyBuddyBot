mod dispatch;
mod families;
mod lists;
mod module;
mod notifications;
mod ops;
mod session;
mod ui;
mod wishes;

use wishlist_core::WishlistError;

#[cxx::bridge(namespace = "wishlist")]
mod ffi {
    #[namespace = "mbb"]
    struct CxxInlineButton {
        label: String,
        callback_data: String,
    }

    unsafe extern "C++" {
        include!("modules/services/ModuleOps.h");

        #[namespace = "mbb"]
        type ModuleOps;

        fn send_text(self: &ModuleOps, chat_id: i64, thread_id: i32, text: &str);
        fn send_with_inline_buttons(
            self: &ModuleOps,
            chat_id: i64,
            thread_id: i32,
            text: &str,
            buttons: &[CxxInlineButton],
            row_sizes: &[i32],
        ) -> i32;
        fn edit_text(self: &ModuleOps, chat_id: i64, message_id: i32, text: &str);
        fn edit_with_inline_buttons(
            self: &ModuleOps,
            chat_id: i64,
            message_id: i32,
            text: &str,
            buttons: &[CxxInlineButton],
            row_sizes: &[i32],
        );
        fn answer_callback(self: &ModuleOps, callback_query_id: &str, text: &str);
        fn delete_message(self: &ModuleOps, chat_id: i64, message_id: i32);
        fn send_text_and_remove_reply_keyboard(
            self: &ModuleOps,
            chat_id: i64,
            thread_id: i32,
            text: &str,
        );

        // User registry lookups (delegates to C++ Storage)
        fn lookup_user_by_username(self: &ModuleOps, username: &str) -> i64;
        fn get_user_display_name(self: &ModuleOps, user_id: i64) -> String;

        #[allow(dead_code)]
        #[namespace = "mbb"]
        fn log_info(msg: &str);
        #[allow(dead_code)]
        #[namespace = "mbb"]
        fn log_debug(msg: &str);
    }

    extern "Rust" {
        type WishlistModule;

        fn create_wishlist_module(db_path: &str) -> Result<Box<WishlistModule>>;
        fn get_trigger_keywords(module: &WishlistModule) -> Vec<String>;
        fn get_callback_prefix(module: &WishlistModule) -> String;
        fn handle_trigger(
            module: &WishlistModule,
            ops: &ModuleOps,
            chat_id: i64,
            thread_id: i32,
            user_id: i64,
            username: &str,
            message_id: i32,
        );
        #[allow(clippy::too_many_arguments)]
        fn handle_callback(
            module: &WishlistModule,
            ops: &ModuleOps,
            chat_id: i64,
            thread_id: i32,
            message_id: i32,
            user_id: i64,
            callback_data: &str,
            callback_query_id: &str,
        );
        fn handle_text_input(
            module: &WishlistModule,
            ops: &ModuleOps,
            chat_id: i64,
            thread_id: i32,
            user_id: i64,
            text: &str,
            message_id: i32,
        ) -> bool;
        fn is_session_active(
            module: &WishlistModule,
            chat_id: i64,
            thread_id: i32,
            user_id: i64,
        ) -> bool;
        fn deactivate_session(module: &WishlistModule, chat_id: i64, thread_id: i32, user_id: i64);
    }
}

pub(crate) use module::WishlistModule;
pub(crate) use notifications::send_user_notification;
pub(crate) use ops::{ModuleOpsLike, log_debug, log_info};
pub(crate) use session::InputState;
pub(crate) use ui::{Dest, btn};

#[cfg(test)]
mod adapter_tests;

#[cfg(test)]
pub(crate) use ops::{FakeModuleOps, RecordedOp};

fn create_wishlist_module(db_path: &str) -> Result<Box<WishlistModule>, WishlistError> {
    module::create_wishlist_module(db_path)
}

fn get_trigger_keywords(module: &WishlistModule) -> Vec<String> {
    module::get_trigger_keywords(module)
}

fn get_callback_prefix(module: &WishlistModule) -> String {
    module::get_callback_prefix(module)
}

fn handle_trigger(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    chat_id: i64,
    thread_id: i32,
    user_id: i64,
    username: &str,
    message_id: i32,
) {
    dispatch::handle_trigger(
        module, ops, chat_id, thread_id, user_id, username, message_id,
    )
}

#[allow(clippy::too_many_arguments)]
fn handle_callback(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    chat_id: i64,
    thread_id: i32,
    message_id: i32,
    user_id: i64,
    callback_data: &str,
    callback_query_id: &str,
) {
    dispatch::handle_callback(
        module,
        ops,
        chat_id,
        thread_id,
        message_id,
        user_id,
        callback_data,
        callback_query_id,
    )
}

fn handle_text_input(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    chat_id: i64,
    thread_id: i32,
    user_id: i64,
    text: &str,
    message_id: i32,
) -> bool {
    dispatch::handle_text_input(module, ops, chat_id, thread_id, user_id, text, message_id)
}

fn is_session_active(module: &WishlistModule, chat_id: i64, thread_id: i32, user_id: i64) -> bool {
    module
        .sessions
        .is_session_active(chat_id, thread_id, user_id)
}

fn deactivate_session(module: &WishlistModule, chat_id: i64, thread_id: i32, user_id: i64) {
    module
        .sessions
        .deactivate_session(chat_id, thread_id, user_id);
}
