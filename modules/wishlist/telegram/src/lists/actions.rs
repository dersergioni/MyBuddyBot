use wishlist_core::WishlistError;

use crate::{Dest, WishlistModule, btn};

use super::render;

pub(crate) fn create(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    name: &str,
    is_private: bool,
) -> bool {
    match module
        .service
        .lists()
        .create_list(owner_id, name, is_private)
    {
        Ok(list) => {
            let prefix = if list.is_private { "🔒 " } else { "" };
            dest.menu(ops, &format!("✅ List \"{prefix}{}\" created!", list.name));
            true
        }
        Err(WishlistError::Validation(_)) => {
            let buttons = vec![btn("❌ Cancel", "wl:menu:show")];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Name cannot be empty. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
        Err(e) => {
            crate::log_info(&format!("lists::create error: {e}"));
            let buttons = vec![btn("❌ Cancel", "wl:menu:show")];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to create list. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(crate) fn rename(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    text: &str,
) -> bool {
    match module.service.lists().rename_list(owner_id, list_id, text) {
        Ok(list) => {
            dest.menu(ops, &format!("✅ List renamed to \"{}\"!", list.name));
            true
        }
        Err(WishlistError::Validation(_)) => {
            let buttons = vec![btn("❌ Cancel", &format!("wl:list:settings:{list_id}"))];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Name cannot be empty. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
        Err(WishlistError::NotFound) => {
            dest.menu(ops, "❌ Could not rename list.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("lists::rename error: {e}"));
            let buttons = vec![btn("❌ Cancel", &format!("wl:list:settings:{list_id}"))];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to rename list. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(super) fn handle_toggle_privacy(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
) {
    match module
        .service
        .lists()
        .toggle_list_privacy(owner_id, list_id)
    {
        Ok(_) => {
            render::render_settings(module, ops, dest, owner_id, list_id);
        }
        Err(WishlistError::NotFound) => {
            dest.text(ops, "❌ List not found.");
        }
        Err(e) => {
            crate::log_info(&format!("lists::toggle_privacy error: {e}"));
            dest.text(ops, "❌ Failed to toggle privacy.");
        }
    }
}

pub(super) fn handle_delete(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
) {
    match module.service.lists().delete_list(owner_id, list_id) {
        Ok(_) => {
            render::render_lists(module, ops, dest, owner_id);
        }
        Err(WishlistError::NotFound) => {
            dest.text(ops, "❌ List not found.");
        }
        Err(e) => {
            crate::log_info(&format!("lists::delete error: {e}"));
            dest.text(ops, "❌ Failed to delete list.");
        }
    }
}
