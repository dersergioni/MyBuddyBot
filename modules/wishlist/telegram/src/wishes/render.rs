use wishlist_core::wishes::Wish;

use crate::{Dest, InputState, WishlistModule, btn, ffi};

pub(crate) fn priority_label(priority: i32) -> &'static str {
    match priority {
        1 => "🟢 Low",
        2 => "🟡 Medium",
        3 => "🔴 High",
        _ => unreachable!("invalid priority value: {priority}"),
    }
}

pub(crate) fn format_wish_detail(wish: &Wish) -> String {
    let mut text = format!("📝 {}", wish.title);
    if let Some(ref url) = wish.url {
        text.push_str(&format!("\n🔗 {url}"));
    }
    if let Some(priority) = wish.priority {
        text.push_str(&format!("\n{}", priority_label(priority)));
    }
    if let Some(ref notes) = wish.notes {
        text.push_str(&format!("\n📌 {notes}"));
    }
    text
}

pub(super) fn render_detail(
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
        .get_owner_wish(owner_id, list_id, wish_id)
    {
        Ok(Some(wish)) => {
            let mut text = format_wish_detail(&wish);
            if wish.reserved_by.is_some() {
                text.push_str("\n\n🔒 Reserved");
            }

            let url_label = if wish.url.is_some() {
                "🔗 Edit URL"
            } else {
                "🔗 Set URL"
            };
            let notes_label = if wish.notes.is_some() {
                "📌 Edit notes"
            } else {
                "📌 Set notes"
            };
            let reservation_label = match wish.reserved_by {
                None => Some("🛍 I'll buy it myself"),
                Some(reserved_user_id) if reserved_user_id == owner_id => {
                    Some("↩️ Make it available")
                }
                Some(_) => None,
            };
            let mut buttons = vec![
                btn(
                    "✏️ Edit title",
                    &format!("wl:wish:edit:title:{list_id}:{wish_id}"),
                ),
                btn(url_label, &format!("wl:wish:edit:url:{list_id}:{wish_id}")),
                btn(
                    "⭐ Priority",
                    &format!("wl:wish:edit:prio:{list_id}:{wish_id}"),
                ),
                btn(
                    notes_label,
                    &format!("wl:wish:edit:notes:{list_id}:{wish_id}"),
                ),
                btn("🗑 Delete", &format!("wl:wish:del:{list_id}:{wish_id}")),
            ];
            if let Some(res_label) = reservation_label {
                buttons.push(btn(
                    res_label,
                    &format!("wl:wish:toggle_reservation:{list_id}:{wish_id}"),
                ));
            }
            buttons.push(btn("⬅️ Back", &format!("wl:list:wishes:{list_id}")));
            let row_sizes: &[i32] = if reservation_label.is_some() {
                &[2, 2, 2, 1]
            } else {
                &[2, 2, 1, 1]
            };
            dest.buttons(ops, &text, &buttons, row_sizes);
        }
        Ok(None) => {
            let buttons = vec![btn("⬅️ Back", &format!("wl:list:wishes:{list_id}"))];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Wish not found.", &buttons, &row_sizes);
        }
        Err(e) => {
            crate::log_info(&format!("wishes::render_detail error: {e}"));
            let buttons = vec![btn("⬅️ Back", &format!("wl:list:wishes:{list_id}"))];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load wish.", &buttons, &row_sizes);
        }
    }
}

pub(super) fn render_create_priority_step(
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    list_id: i64,
    wish_id: i64,
) {
    let buttons = vec![
        btn(
            "🟢 Low",
            &format!("wl:wish:create:prio:{list_id}:{wish_id}:1"),
        ),
        btn(
            "🟡 Medium",
            &format!("wl:wish:create:prio:{list_id}:{wish_id}:2"),
        ),
        btn(
            "🔴 High",
            &format!("wl:wish:create:prio:{list_id}:{wish_id}:3"),
        ),
        btn(
            "Skip priority",
            &format!("wl:wish:create:prio:{list_id}:{wish_id}:0"),
        ),
        btn("Done for now", &format!("wl:wish:create:done:{list_id}")),
    ];
    let row_sizes = [3, 1, 1];
    dest.buttons(
        ops,
        "⭐ Choose a priority for your new wish, or continue without one:",
        &buttons,
        &row_sizes,
    );
}

pub(super) fn render_create_url_step(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
) {
    module.sessions.set_input_state(
        dest.session_key(owner_id),
        InputState::WishCreateUrl { list_id, wish_id },
    );

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
        "🔗 Enter a URL for this wish, or choose an option below:",
        &buttons,
        &row_sizes,
    );
}

pub(super) fn render_create_notes_step(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
    wish_id: i64,
) {
    module.sessions.set_input_state(
        dest.session_key(owner_id),
        InputState::WishCreateNotes { list_id, wish_id },
    );

    let buttons = vec![btn(
        "Done for now",
        &format!("wl:wish:create:done:{list_id}"),
    )];
    let row_sizes = [1];
    dest.buttons(
        ops,
        "📌 Enter notes for this wish, or finish for now:",
        &buttons,
        &row_sizes,
    );
}

pub(super) fn render_list(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
) {
    let list = match module.service.lists().get_owner_list(owner_id, list_id) {
        Ok(Some(info)) => info,
        Ok(None) => {
            let buttons = vec![btn("⬅️ Back", "wl:menu:show")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ List not found.", &buttons, &row_sizes);
            return;
        }
        Err(e) => {
            crate::log_info(&format!("wishes::show error: {e}"));
            let buttons = vec![btn("⬅️ Back", "wl:menu:show")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load list.", &buttons, &row_sizes);
            return;
        }
    };

    let prefix = if list.is_private { "🔒 " } else { "" };

    match module.service.wishes().list_wishes(list_id) {
        Ok(wishes) => {
            let wish_count = wishes.len();
            let mut buttons: Vec<ffi::CxxInlineButton> = wishes
                .iter()
                .enumerate()
                .map(|(idx, w)| {
                    let mut label = format!("{}. {}", idx + 1, w.title);
                    if let Some(p) = w.priority {
                        let emoji = match p {
                            1 => " 🟢",
                            2 => " 🟡",
                            3 => " 🔴",
                            _ => "",
                        };
                        label.push_str(emoji);
                    }

                    let mut extras = Vec::new();
                    if let Some(ref url) = w.url
                        && !url.trim().is_empty()
                    {
                        let truncated: String = url.chars().take(40).collect();
                        extras.push(format!("🔗 {truncated}"));
                    }
                    if let Some(ref notes) = w.notes
                        && !notes.trim().is_empty()
                    {
                        extras.push(format!("📌{notes}"));
                    }
                    if !extras.is_empty() {
                        label.push_str(&format!(" [{}]", extras.join(", ")));
                    }

                    if w.reserved_by.is_some() {
                        label.push_str(" 🔒");
                    }
                    btn(&label, &format!("wl:wish:view:{list_id}:{}", w.id))
                })
                .collect();
            buttons.push(btn(
                "➕ Add wish",
                &format!("wl:wish:add-from-list:{list_id}"),
            ));
            buttons.push(btn("⬅️ Back", "wl:menu:show"));
            buttons.push(btn("⚙️ Settings", &format!("wl:list:settings:{list_id}")));

            let mut row_sizes: Vec<i32> = vec![1; wish_count];
            row_sizes.push(1);
            row_sizes.push(2);

            let header = if wishes.is_empty() {
                format!("{prefix}{}: empty", list.name)
            } else {
                format!("{prefix}{}:", list.name)
            };
            dest.buttons(ops, &header, &buttons, &row_sizes);
        }
        Err(e) => {
            crate::log_info(&format!("wishes::show error: {e}"));
            let buttons = vec![btn("⬅️ Back", "wl:menu:show")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load wishes.", &buttons, &row_sizes);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{format_wish_detail, priority_label};
    use wishlist_core::wishes::Wish;

    #[test]
    fn priority_label_matches_supported_values() {
        assert_eq!(priority_label(1), "🟢 Low");
        assert_eq!(priority_label(2), "🟡 Medium");
        assert_eq!(priority_label(3), "🔴 High");
    }

    #[test]
    fn format_wish_detail_includes_present_optional_fields() {
        let wish = Wish {
            id: 7,
            title: "Camera".to_string(),
            url: Some("https://example.test".to_string()),
            priority: Some(2),
            notes: Some("Black body".to_string()),
            reserved_by: None,
        };

        assert_eq!(
            format_wish_detail(&wish),
            "📝 Camera\n🔗 https://example.test\n🟡 Medium\n📌 Black body"
        );
    }
}
