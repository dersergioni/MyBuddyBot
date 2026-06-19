use crate::session::{InputState, SessionKey};
use crate::ui::{Dest, MENU_PROMPT, MENU_ROW_SIZES, menu_buttons};
use crate::{WishlistModule, families, lists, wishes};

fn is_callback_allowed_when_inactive(callback_data: &str) -> bool {
    callback_data.starts_with(families::ACCEPT_PREFIX)
        || callback_data.starts_with(families::DECLINE_PREFIX)
}

pub(crate) fn handle_trigger(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    chat_id: i64,
    thread_id: i32,
    user_id: i64,
    username: &str,
    _message_id: i32,
) {
    let key = SessionKey::new(chat_id, thread_id, user_id);
    crate::log_info(&format!(
        "Wishlist trigger: user_id={user_id} username='{username}'"
    ));
    module.sessions.touch(key);

    let msg_id = ops.send_with_inline_buttons(
        chat_id,
        thread_id,
        MENU_PROMPT,
        &menu_buttons(),
        &MENU_ROW_SIZES,
    );
    if msg_id != 0 {
        module.sessions.remember_active_message(key, msg_id);
    }
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn handle_callback(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    chat_id: i64,
    thread_id: i32,
    message_id: i32,
    user_id: i64,
    callback_data: &str,
    callback_query_id: &str,
) {
    let key = SessionKey::new(chat_id, thread_id, user_id);
    crate::log_debug(&format!(
        "Wishlist callback: user_id={user_id} data='{callback_data}'"
    ));

    if module.sessions.is_active(key) || !is_callback_allowed_when_inactive(callback_data) {
        crate::log_debug(&format!(
            "Wishlist: re-activating user_id={user_id} on callback"
        ));
        module.sessions.touch(key);
    }

    module.sessions.clear_input_state(key);
    module.sessions.remember_active_message(key, message_id);

    let dest = Dest::edit(chat_id, thread_id, message_id);

    match callback_data {
        "wl:menu:show" => {
            ops.answer_callback(callback_query_id, "");
            lists::show(module, ops, &dest, user_id);
        }
        "wl:menu:add" => {
            ops.answer_callback(callback_query_id, "");
            lists::show_for_adding(module, ops, &dest, user_id);
        }
        "wl:menu:family" => {
            ops.answer_callback(callback_query_id, "");
            families::show(module, ops, &dest, user_id);
        }
        "wl:menu:back" => {
            ops.answer_callback(callback_query_id, "");
            dest.menu(ops, MENU_PROMPT);
        }
        "wl:menu:exit" => {
            module.sessions.clear(key);
            ops.answer_callback(callback_query_id, "");
            ops.delete_message(chat_id, message_id);
            ops.send_text_and_remove_reply_keyboard(
                chat_id,
                thread_id,
                "👋 Wishlist closed. Type \"wishlist\" to reopen.",
            );
        }
        data if data.starts_with("wl:list:") => {
            lists::handle_callback(
                module,
                ops,
                &dest,
                user_id,
                callback_data,
                callback_query_id,
            );
        }
        data if data.starts_with("wl:wish:") => {
            wishes::handle_callback(
                module,
                ops,
                &dest,
                user_id,
                callback_data,
                callback_query_id,
            );
        }
        data if data.starts_with("wl:fam:") => {
            families::handle_callback(
                module,
                ops,
                &dest,
                user_id,
                callback_data,
                callback_query_id,
            );
        }
        _ => {
            crate::log_info(&format!("Wishlist: unknown callback '{callback_data}'"));
            ops.answer_callback(callback_query_id, "Unknown action");
        }
    }
}

pub(crate) fn handle_text_input(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    chat_id: i64,
    thread_id: i32,
    user_id: i64,
    text: &str,
    _message_id: i32,
) -> bool {
    let key = SessionKey::new(chat_id, thread_id, user_id);

    if !module.sessions.is_active(key) {
        return false;
    }

    module.sessions.touch(key);

    if let Some(message_id) = module.sessions.take_active_message(key) {
        ops.delete_message(chat_id, message_id);
    }

    let dest = Dest::send(chat_id, thread_id);
    let state = module.sessions.take_input_state(key);

    let handled = match state {
        Some(InputState::ListName { is_private }) => {
            if !lists::create(module, ops, &dest, user_id, text, is_private) {
                module
                    .sessions
                    .set_input_state(key, InputState::ListName { is_private });
            }
            true
        }
        Some(InputState::ListRename { list_id }) => {
            if !lists::rename(module, ops, &dest, user_id, list_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::ListRename { list_id });
            }
            true
        }
        Some(InputState::WishTitle { list_id }) => {
            if !wishes::create(module, ops, &dest, user_id, list_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::WishTitle { list_id });
            }
            true
        }
        Some(InputState::WishCreateUrl { list_id, wish_id }) => {
            if !wishes::create_url(module, ops, &dest, user_id, list_id, wish_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::WishCreateUrl { list_id, wish_id });
            }
            true
        }
        Some(InputState::WishCreateNotes { list_id, wish_id }) => {
            if !wishes::create_notes(module, ops, &dest, user_id, list_id, wish_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::WishCreateNotes { list_id, wish_id });
            }
            true
        }
        Some(InputState::WishEditTitle { list_id, wish_id }) => {
            if !wishes::edit_title(module, ops, &dest, user_id, list_id, wish_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::WishEditTitle { list_id, wish_id });
            }
            true
        }
        Some(InputState::WishEditUrl { list_id, wish_id }) => {
            if !wishes::edit_url(module, ops, &dest, user_id, list_id, wish_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::WishEditUrl { list_id, wish_id });
            }
            true
        }
        Some(InputState::WishEditNotes { list_id, wish_id }) => {
            if !wishes::edit_notes(module, ops, &dest, user_id, list_id, wish_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::WishEditNotes { list_id, wish_id });
            }
            true
        }
        Some(InputState::FamilyName) => {
            if !families::create(module, ops, &dest, user_id, text) {
                module.sessions.set_input_state(key, InputState::FamilyName);
            }
            true
        }
        Some(InputState::FamilyMember { family_id }) => {
            if !families::add_member(module, ops, &dest, user_id, family_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::FamilyMember { family_id });
            }
            true
        }
        Some(InputState::FamilyRename { family_id }) => {
            if !families::rename(module, ops, &dest, user_id, family_id, text) {
                module
                    .sessions
                    .set_input_state(key, InputState::FamilyRename { family_id });
            }
            true
        }
        None => {
            if module.sessions.is_active(key) {
                dest.menu(ops, "Use the buttons below 👇");
                true
            } else {
                false
            }
        }
    };

    let msg_id = dest.last_sent_id();
    if msg_id != 0 {
        module.sessions.remember_active_message(key, msg_id);
    }

    handled
}

#[cfg(test)]
mod tests {
    use super::is_callback_allowed_when_inactive;

    #[test]
    fn inactive_callback_allowlist_is_limited_to_invitation_actions() {
        assert!(is_callback_allowed_when_inactive("wl:fam:accept:42"));
        assert!(is_callback_allowed_when_inactive("wl:fam:decline:42"));
        assert!(!is_callback_allowed_when_inactive("wl:menu:show"));
        assert!(!is_callback_allowed_when_inactive("wl:fam:leave"));
    }
}
