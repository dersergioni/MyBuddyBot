use wishlist_core::WishlistError;

use crate::{Dest, WishlistModule, btn};

use super::render;

fn parse_optional_text(text: &str) -> Option<&str> {
    let trimmed = text.trim();
    if trimmed.is_empty() || trimmed == "-" {
        None
    } else {
        Some(trimmed)
    }
}

pub(crate) fn create(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    title: &str,
) -> bool {
    match module
        .service
        .wishes()
        .create_wish(owner_id, list_id, title)
    {
        Ok(wish_id) => {
            render::render_create_priority_step(ops, dest, list_id, wish_id);
            true
        }
        Err(WishlistError::Validation(_)) => {
            let buttons = vec![btn("❌ Cancel", &format!("wl:list:wishes:{list_id}"))];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Title cannot be empty. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
        Err(WishlistError::NotFound) => {
            dest.menu(ops, "❌ List not found.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("wishes::create error: {e}"));
            let buttons = vec![btn("❌ Cancel", &format!("wl:list:wishes:{list_id}"))];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to add wish. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(crate) fn create_url(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
    text: &str,
) -> bool {
    let url = parse_optional_text(text);

    match module
        .service
        .wishes()
        .update_url(owner_id, list_id, wish_id, url)
    {
        Ok(_) => {
            render::render_create_notes_step(module, ops, dest, owner_id, list_id, wish_id);
            true
        }
        Err(WishlistError::NotFound) => {
            dest.menu(ops, "❌ Wish not found.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("wishes::create_url error: {e}"));
            let buttons = vec![
                btn(
                    "Skip URL",
                    &format!("wl:wish:create:url:skip:{list_id}:{wish_id}"),
                ),
                btn("Done for now", &format!("wl:wish:create:done:{list_id}")),
            ];
            let row_sizes = [1, 1];
            dest.buttons(
                ops,
                "❌ Failed to save URL. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(crate) fn create_notes(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
    text: &str,
) -> bool {
    let notes = parse_optional_text(text);

    match module
        .service
        .wishes()
        .update_notes(owner_id, list_id, wish_id, notes)
    {
        Ok(_) => {
            render::render_list(module, ops, dest, owner_id, list_id);
            true
        }
        Err(WishlistError::NotFound) => {
            dest.menu(ops, "❌ Wish not found.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("wishes::create_notes error: {e}"));
            let buttons = vec![btn(
                "Done for now",
                &format!("wl:wish:create:done:{list_id}"),
            )];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to save notes. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(crate) fn edit_title(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
    text: &str,
) -> bool {
    match module
        .service
        .wishes()
        .update_title(owner_id, list_id, wish_id, text)
    {
        Ok(_) => {
            render::render_detail(module, ops, dest, owner_id, list_id, wish_id);
            true
        }
        Err(WishlistError::Validation(_)) => {
            let buttons = vec![btn(
                "❌ Cancel",
                &format!("wl:wish:view:{list_id}:{wish_id}"),
            )];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Title cannot be empty. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
        Err(WishlistError::NotFound) => {
            dest.menu(ops, "❌ Wish not found.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("wishes::edit_title error: {e}"));
            let buttons = vec![btn(
                "❌ Cancel",
                &format!("wl:wish:view:{list_id}:{wish_id}"),
            )];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to update title. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(crate) fn edit_url(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
    text: &str,
) -> bool {
    let url = parse_optional_text(text);
    match module
        .service
        .wishes()
        .update_url(owner_id, list_id, wish_id, url)
    {
        Ok(_) => {
            render::render_detail(module, ops, dest, owner_id, list_id, wish_id);
            true
        }
        Err(WishlistError::NotFound) => {
            dest.menu(ops, "❌ Wish not found.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("wishes::edit_url error: {e}"));
            let buttons = vec![btn(
                "❌ Cancel",
                &format!("wl:wish:view:{list_id}:{wish_id}"),
            )];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to update URL. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(crate) fn edit_notes(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
    text: &str,
) -> bool {
    let notes = parse_optional_text(text);
    match module
        .service
        .wishes()
        .update_notes(owner_id, list_id, wish_id, notes)
    {
        Ok(_) => {
            render::render_detail(module, ops, dest, owner_id, list_id, wish_id);
            true
        }
        Err(WishlistError::NotFound) => {
            dest.menu(ops, "❌ Wish not found.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("wishes::edit_notes error: {e}"));
            let buttons = vec![btn(
                "❌ Cancel",
                &format!("wl:wish:view:{list_id}:{wish_id}"),
            )];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to update notes. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(super) fn delete(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
) {
    match module
        .service
        .wishes()
        .delete_wish(owner_id, list_id, wish_id)
    {
        Ok(_) => {
            render::render_list(module, ops, dest, owner_id, list_id);
        }
        Err(WishlistError::NotFound) => {
            dest.text(ops, "❌ Wish not found.");
        }
        Err(e) => {
            crate::log_info(&format!("wishes::delete error: {e}"));
            dest.text(ops, "❌ Failed to delete.");
        }
    }
}

pub(super) fn set_priority(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
    priority: Option<i32>,
) {
    match module
        .service
        .wishes()
        .update_priority(owner_id, list_id, wish_id, priority)
    {
        Ok(_) => {
            render::render_detail(module, ops, dest, owner_id, list_id, wish_id);
        }
        Err(WishlistError::NotFound) => {
            dest.text(ops, "❌ Wish not found.");
        }
        Err(e) => {
            crate::log_info(&format!("wishes::set_priority error: {e}"));
            dest.text(ops, "❌ Failed to set priority.");
        }
    }
}

pub(super) fn create_priority(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
    priority: Option<i32>,
) {
    match module
        .service
        .wishes()
        .update_priority(owner_id, list_id, wish_id, priority)
    {
        Ok(_) => {
            render::render_create_url_step(module, ops, dest, owner_id, list_id, wish_id);
        }
        Err(WishlistError::NotFound) => {
            dest.text(ops, "❌ Wish not found.");
        }
        Err(e) => {
            crate::log_info(&format!("wishes::create_set_priority error: {e}"));
            dest.text(ops, "❌ Failed to save priority.");
        }
    }
}

pub(super) fn toggle_reservation(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
) {
    match module
        .service
        .wishes()
        .toggle_owner_reservation(owner_id, list_id, wish_id)
    {
        Ok(_) => {
            render::render_detail(module, ops, dest, owner_id, list_id, wish_id);
        }
        Err(WishlistError::NotFound) => {
            crate::log_info("wishes::toggle_reservation error: wish not found");
            dest.text(ops, "❌ Wish not found.");
        }
        Err(e) => {
            crate::log_info(&format!("wishes::toggle_reservation error: {e}"));
            dest.text(ops, "❌ Failed to update reservation.");
        }
    }
}

#[cfg(test)]
mod tests {
    use super::parse_optional_text;

    #[test]
    fn parse_optional_text_treats_empty_and_dash_as_none() {
        assert_eq!(parse_optional_text(""), None);
        assert_eq!(parse_optional_text("   "), None);
        assert_eq!(parse_optional_text("-"), None);
        assert_eq!(parse_optional_text("  -  "), None);
    }

    #[test]
    fn parse_optional_text_trims_content() {
        assert_eq!(parse_optional_text("  hello  "), Some("hello"));
    }
}
