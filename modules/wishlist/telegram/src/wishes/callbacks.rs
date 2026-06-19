use crate::{Dest, InputState, WishlistModule, btn};

use super::actions;
use super::render;

enum CancelTarget {
    Menu,
    List,
}

pub(crate) fn handle_callback(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    callback_data: &str,
    callback_query_id: &str,
) {
    if let Some(rest) = callback_data.strip_prefix("wl:wish:view:") {
        ops.answer_callback(callback_query_id, "");
        cb_view(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:del:yes:") {
        ops.answer_callback(callback_query_id, "");
        cb_delete_confirm(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:del:") {
        ops.answer_callback(callback_query_id, "");
        cb_delete_prompt(ops, dest, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:edit:title:") {
        ops.answer_callback(callback_query_id, "");
        cb_edit_title(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:edit:url:") {
        ops.answer_callback(callback_query_id, "");
        cb_edit_url(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:edit:notes:") {
        ops.answer_callback(callback_query_id, "");
        cb_edit_notes(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:edit:prio:") {
        ops.answer_callback(callback_query_id, "");
        cb_edit_priority(ops, dest, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:prio:") {
        ops.answer_callback(callback_query_id, "");
        cb_set_priority(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:create:prio:") {
        ops.answer_callback(callback_query_id, "");
        cb_create_priority(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:create:url:skip:") {
        ops.answer_callback(callback_query_id, "");
        cb_create_url_skip(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:create:url:") {
        ops.answer_callback(callback_query_id, "");
        cb_create_url_prompt(module, ops, dest, owner_id, rest);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:create:notes:") {
        ops.answer_callback(callback_query_id, "");
        cb_create_notes(module, ops, dest, owner_id, rest);
    } else if let Some(list_id_str) = callback_data.strip_prefix("wl:wish:create:done:") {
        ops.answer_callback(callback_query_id, "");
        cb_create_done(module, ops, dest, owner_id, list_id_str);
    } else if let Some(list_id_str) = callback_data.strip_prefix("wl:wish:add-from-list:") {
        ops.answer_callback(callback_query_id, "");
        cb_add(module, ops, dest, owner_id, list_id_str, CancelTarget::List);
    } else if let Some(list_id_str) = callback_data.strip_prefix("wl:wish:add-from-menu:") {
        ops.answer_callback(callback_query_id, "");
        cb_add(module, ops, dest, owner_id, list_id_str, CancelTarget::Menu);
    } else if let Some(rest) = callback_data.strip_prefix("wl:wish:toggle_reservation:") {
        ops.answer_callback(callback_query_id, "");
        cb_toggle_reservation(module, ops, dest, owner_id, rest);
    } else {
        ops.answer_callback(callback_query_id, "Unknown wish action");
    }
}

fn cb_view(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        render::render_detail(module, ops, dest, owner_id, list_id, wish_id);
    }
}

fn cb_delete_confirm(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        actions::delete(module, ops, dest, owner_id, list_id, wish_id);
    }
}

fn cb_delete_prompt(ops: &dyn crate::ModuleOpsLike, dest: &Dest, rest: &str) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        let buttons = vec![
            btn(
                "✅ Yes, delete",
                &format!("wl:wish:del:yes:{list_id}:{wish_id}"),
            ),
            btn("❌ Cancel", &format!("wl:wish:view:{list_id}:{wish_id}")),
        ];
        let row_sizes = [2];
        dest.buttons(ops, "⚠️ Delete this wish?", &buttons, &row_sizes);
    }
}

fn cb_edit_title(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        module.sessions.set_input_state(
            dest.session_key(owner_id),
            InputState::WishEditTitle { list_id, wish_id },
        );
        let buttons = vec![btn(
            "❌ Cancel",
            &format!("wl:wish:view:{list_id}:{wish_id}"),
        )];
        let row_sizes = [1];
        dest.buttons(ops, "📝 Enter the new title:", &buttons, &row_sizes);
    }
}

fn cb_edit_url(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        module.sessions.set_input_state(
            dest.session_key(owner_id),
            InputState::WishEditUrl { list_id, wish_id },
        );
        let buttons = vec![btn(
            "❌ Cancel",
            &format!("wl:wish:view:{list_id}:{wish_id}"),
        )];
        let row_sizes = [1];
        dest.buttons(
            ops,
            "🔗 Enter the URL (or \"-\" to remove):",
            &buttons,
            &row_sizes,
        );
    }
}

fn cb_edit_notes(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        module.sessions.set_input_state(
            dest.session_key(owner_id),
            InputState::WishEditNotes { list_id, wish_id },
        );
        let buttons = vec![btn(
            "❌ Cancel",
            &format!("wl:wish:view:{list_id}:{wish_id}"),
        )];
        let row_sizes = [1];
        dest.buttons(
            ops,
            "📌 Enter notes (or \"-\" to remove):",
            &buttons,
            &row_sizes,
        );
    }
}

fn cb_edit_priority(ops: &dyn crate::ModuleOpsLike, dest: &Dest, rest: &str) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        let buttons = vec![
            btn("🟢 Low", &format!("wl:wish:prio:{list_id}:{wish_id}:1")),
            btn("🟡 Medium", &format!("wl:wish:prio:{list_id}:{wish_id}:2")),
            btn("🔴 High", &format!("wl:wish:prio:{list_id}:{wish_id}:3")),
            btn("❌ Clear", &format!("wl:wish:prio:{list_id}:{wish_id}:0")),
            btn("⬅️ Back", &format!("wl:wish:view:{list_id}:{wish_id}")),
        ];
        let row_sizes = [3, 1, 1];
        dest.buttons(ops, "⭐ Set priority:", &buttons, &row_sizes);
    }
}

fn cb_set_priority(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    let parts: Vec<&str> = rest.splitn(3, ':').collect();
    if parts.len() == 3
        && let (Ok(list_id), Ok(wish_id), Ok(value)) = (
            parts[0].parse::<i64>(),
            parts[1].parse::<i64>(),
            parts[2].parse::<i32>(),
        )
    {
        let priority = if value == 0 { None } else { Some(value) };
        actions::set_priority(module, ops, dest, owner_id, list_id, wish_id, priority);
    }
}

fn cb_create_priority(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    let parts: Vec<&str> = rest.splitn(3, ':').collect();
    if parts.len() == 3
        && let (Ok(list_id), Ok(wish_id), Ok(value)) = (
            parts[0].parse::<i64>(),
            parts[1].parse::<i64>(),
            parts[2].parse::<i32>(),
        )
    {
        let priority = if value == 0 { None } else { Some(value) };
        actions::create_priority(module, ops, dest, owner_id, list_id, wish_id, priority);
    }
}

fn cb_create_url_skip(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        render::render_create_notes_step(module, ops, dest, owner_id, list_id, wish_id);
    }
}

fn cb_create_url_prompt(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        render::render_create_url_step(module, ops, dest, owner_id, list_id, wish_id);
    }
}

fn cb_create_notes(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        render::render_create_notes_step(module, ops, dest, owner_id, list_id, wish_id);
    }
}

fn cb_create_done(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id_str: &str,
) {
    if let Ok(list_id) = list_id_str.parse::<i64>() {
        render::render_list(module, ops, dest, owner_id, list_id);
    }
}

fn cb_add(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id_str: &str,
    cancel: CancelTarget,
) {
    if let Ok(list_id) = list_id_str.parse::<i64>() {
        module.sessions.set_input_state(
            dest.session_key(owner_id),
            InputState::WishTitle { list_id },
        );
        let cancel_cb = match cancel {
            CancelTarget::Menu => "wl:menu:add".to_string(),
            CancelTarget::List => format!("wl:list:wishes:{list_id}"),
        };
        let buttons = vec![btn("❌ Cancel", &cancel_cb)];
        let row_sizes = [1];
        dest.buttons(
            ops,
            "📝 Enter the title for your new wish:",
            &buttons,
            &row_sizes,
        );
    }
}

fn cb_toggle_reservation(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    rest: &str,
) {
    if let Some((list_str, wish_str)) = rest.split_once(':')
        && let (Ok(list_id), Ok(wish_id)) = (list_str.parse::<i64>(), wish_str.parse::<i64>())
    {
        actions::toggle_reservation(module, ops, dest, owner_id, list_id, wish_id);
    }
}
