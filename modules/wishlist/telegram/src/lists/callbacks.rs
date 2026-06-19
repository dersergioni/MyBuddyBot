use crate::{Dest, InputState, WishlistModule, btn};

use super::{actions, render};

pub(crate) fn handle_callback(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    callback_data: &str,
    callback_query_id: &str,
) {
    match callback_data {
        "wl:list:create" => {
            ops.answer_callback(callback_query_id, "");
            module.sessions.set_input_state(
                dest.session_key(owner_id),
                InputState::ListName { is_private: false },
            );
            let buttons = vec![btn("❌ Cancel", "wl:menu:show")];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "📝 Enter the name for your new list:",
                &buttons,
                &row_sizes,
            );
        }
        _ if callback_data.starts_with("wl:list:wishes:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:list:wishes:")
                && let Ok(list_id) = rest.parse::<i64>()
            {
                crate::wishes::show(module, ops, dest, owner_id, list_id);
            }
        }
        _ if callback_data.starts_with("wl:list:rename:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:list:rename:")
                && let Ok(list_id) = rest.parse::<i64>()
            {
                module.sessions.set_input_state(
                    dest.session_key(owner_id),
                    InputState::ListRename { list_id },
                );
                let buttons = vec![btn("❌ Cancel", &format!("wl:list:settings:{list_id}"))];
                let row_sizes = [1];
                dest.buttons(
                    ops,
                    "📝 Enter the new name for this list:",
                    &buttons,
                    &row_sizes,
                );
            }
        }
        _ if callback_data.starts_with("wl:list:delete:yes:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:list:delete:yes:")
                && let Ok(list_id) = rest.parse::<i64>()
            {
                actions::handle_delete(module, ops, dest, owner_id, list_id);
            }
        }
        _ if callback_data.starts_with("wl:list:delete:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:list:delete:")
                && let Ok(list_id) = rest.parse::<i64>()
            {
                let buttons = vec![
                    btn("✅ Yes, delete", &format!("wl:list:delete:yes:{list_id}")),
                    btn("❌ Cancel", &format!("wl:list:settings:{list_id}")),
                ];
                let row_sizes = [2];
                dest.buttons(
                    ops,
                    "⚠️ Delete this list? All wishes in it will be removed.",
                    &buttons,
                    &row_sizes,
                );
            }
        }
        _ if callback_data.starts_with("wl:list:privacy:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:list:privacy:")
                && let Ok(list_id) = rest.parse::<i64>()
            {
                actions::handle_toggle_privacy(module, ops, dest, owner_id, list_id);
            }
        }
        _ if callback_data.starts_with("wl:list:settings:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:list:settings:")
                && let Ok(list_id) = rest.parse::<i64>()
            {
                render::render_settings(module, ops, dest, owner_id, list_id);
            }
        }
        _ if callback_data.starts_with("wl:list:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:list:")
                && let Ok(list_id) = rest.parse::<i64>()
            {
                crate::wishes::show(module, ops, dest, owner_id, list_id);
            }
        }
        _ => {
            ops.answer_callback(callback_query_id, "Unknown list action");
        }
    }
}
