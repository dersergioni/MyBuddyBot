use crate::WishlistModule;

pub(crate) fn send_user_notification(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    user_id: i64,
    text: &str,
) {
    if let Some(key) = module.sessions.latest_active_session_for_user(user_id) {
        crate::log_debug(&format!(
            "Wishlist notification routed to active session: user_id={} chat_id={} thread_id={}",
            user_id, key.chat_id, key.thread_id
        ));
        ops.send_text(key.chat_id, key.thread_id, text);
    } else {
        crate::log_debug(&format!(
            "Wishlist notification routed to default chat: user_id={user_id}"
        ));
        ops.send_text(user_id, 0, text);
    }
}
