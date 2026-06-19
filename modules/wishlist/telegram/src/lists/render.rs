use crate::{Dest, WishlistModule, btn, ffi};

pub(super) fn render_lists(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
) {
    match module.service.lists().list_owner_lists(owner_id) {
        Ok(lists) if lists.is_empty() => {
            let buttons = vec![
                btn("⬅️ Back", "wl:menu:back"),
                btn("➕ Create list", "wl:list:create"),
            ];
            let row_sizes = [2];
            dest.buttons(ops, "📋 You have no lists yet.", &buttons, &row_sizes);
        }
        Ok(lists) => {
            let mut buttons: Vec<ffi::CxxInlineButton> = lists
                .iter()
                .map(|list| {
                    let prefix = if list.is_private { "🔒 " } else { "" };
                    btn(
                        &format!("{prefix}{}", list.name),
                        &format!("wl:list:{}", list.id),
                    )
                })
                .collect();
            buttons.push(btn("⬅️ Back", "wl:menu:back"));
            buttons.push(btn("➕ Create list", "wl:list:create"));
            let list_count = buttons.len() - 2;
            let mut row_sizes: Vec<i32> = vec![1; list_count];
            row_sizes.push(2);
            dest.buttons(ops, "📋 Your lists:", &buttons, &row_sizes);
        }
        Err(e) => {
            crate::log_info(&format!("lists::show error: {e}"));
            dest.text(ops, "❌ Failed to load lists.");
        }
    }
}

pub(super) fn render_add_target_lists(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
) {
    match module.service.lists().list_owner_lists(owner_id) {
        Ok(lists) if lists.is_empty() => {
            let buttons = vec![
                btn("➕ Create list first", "wl:list:create"),
                btn("⬅️ Back", "wl:menu:back"),
            ];
            let row_sizes = [1, 1];
            dest.buttons(
                ops,
                "You need a list before adding wishes.\nCreate one first!",
                &buttons,
                &row_sizes,
            );
        }
        Ok(lists) => {
            let mut buttons: Vec<ffi::CxxInlineButton> = lists
                .iter()
                .map(|list| {
                    let prefix = if list.is_private { "🔒 " } else { "" };
                    btn(
                        &format!("{prefix}{}", list.name),
                        &format!("wl:wish:add-from-menu:{}", list.id),
                    )
                })
                .collect();
            buttons.push(btn("⬅️ Back", "wl:menu:back"));
            let row_sizes: Vec<i32> = vec![1; buttons.len()];
            dest.buttons(ops, "➕ Add wish — select a list:", &buttons, &row_sizes);
        }
        Err(e) => {
            crate::log_info(&format!("lists::show_for_adding error: {e}"));
            dest.text(ops, "❌ Failed to load lists.");
        }
    }
}

pub(super) fn render_settings(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
) {
    match module.service.lists().get_owner_list(owner_id, list_id) {
        Ok(Some(list)) => {
            let prefix = if list.is_private { "🔒 " } else { "" };
            let privacy_label = if list.is_private {
                "🔓 Make public"
            } else {
                "🔒 Make private"
            };
            let buttons = vec![
                btn("✏️ Rename", &format!("wl:list:rename:{list_id}")),
                btn(privacy_label, &format!("wl:list:privacy:{list_id}")),
                btn("🗑 Delete", &format!("wl:list:delete:{list_id}")),
                btn("⬅️ Back", &format!("wl:list:wishes:{list_id}")),
            ];
            let row_sizes = [1, 1, 1, 1];
            dest.buttons(
                ops,
                &format!("⚙️ {prefix}{}", list.name),
                &buttons,
                &row_sizes,
            );
        }
        Ok(None) => {
            let buttons = vec![btn("⬅️ Back", "wl:menu:show")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ List not found.", &buttons, &row_sizes);
        }
        Err(e) => {
            crate::log_info(&format!("lists::render_settings error: {e}"));
            dest.text(ops, "❌ Failed to load list.");
        }
    }
}
