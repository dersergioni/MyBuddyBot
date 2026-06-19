use wishlist_core::WishlistError;

use crate::{Dest, WishlistModule, btn, ffi};

pub(super) fn show_family_browse_message(ops: &dyn crate::ModuleOpsLike, dest: &Dest, text: &str) {
    let buttons = vec![btn("⬅️ Back", "wl:fam:browse")];
    let row_sizes = [1];
    dest.buttons(ops, text, &buttons, &row_sizes);
}

pub(super) fn render_family(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    viewer_user_id: i64,
) {
    match module
        .service
        .families()
        .get_family_for_user(viewer_user_id)
    {
        Ok(Some(family)) => {
            let is_owner = family.owner_id == viewer_user_id;
            let mut buttons = vec![
                btn("👥 Members", "wl:fam:members"),
                btn("📋 Browse lists", "wl:fam:browse"),
            ];
            if is_owner {
                buttons.push(btn("➕ Add member", "wl:fam:add"));
                buttons.push(btn("✏️ Rename", "wl:fam:rename"));
                buttons.push(btn("🗑 Delete family", "wl:fam:delete"));
            } else {
                buttons.push(btn("🚪 Leave", "wl:fam:leave:confirm"));
                buttons.push(btn("⬅️ Back", "wl:menu:back"));
            }
            if is_owner {
                buttons.push(btn("⬅️ Back", "wl:menu:back"));
            }

            let row_sizes: Vec<i32> = if is_owner {
                vec![2, 2, 1, 1]
            } else {
                vec![2, 2]
            };

            dest.buttons(
                ops,
                &format!("👪 Family: {}", family.name),
                &buttons,
                &row_sizes,
            );
        }
        Ok(None) => match module
            .service
            .families()
            .get_pending_invitation(viewer_user_id)
        {
            Ok(Some(invitation)) => {
                let owner_name = ops.get_user_display_name(invitation.owner_id);
                let buttons = vec![
                    btn(
                        "✅ Accept",
                        &format!("wl:fam:accept:{}", invitation.family_id),
                    ),
                    btn(
                        "❌ Decline",
                        &format!("wl:fam:decline:{}", invitation.family_id),
                    ),
                    btn("⬅️ Back", "wl:menu:back"),
                ];
                let row_sizes = [2, 1];
                dest.buttons(
                    ops,
                    &format!(
                        "📩 {owner_name} invited you to family \"{}\"",
                        invitation.family_name
                    ),
                    &buttons,
                    &row_sizes,
                );
            }
            result => {
                if let Err(e) = result {
                    crate::log_info(&format!(
                        "families::show: get_pending_invitation error: {e}"
                    ));
                }
                let buttons = vec![
                    btn("➕ Create family", "wl:fam:create"),
                    btn("⬅️ Back", "wl:menu:back"),
                ];
                let row_sizes = [1, 1];
                dest.buttons(
                    ops,
                    "👪 You are not in any family yet.",
                    &buttons,
                    &row_sizes,
                );
            }
        },
        Err(e) => {
            crate::log_info(&format!("families::show error: {e}"));
            let buttons = vec![btn("⬅️ Back", "wl:menu:back")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load family.", &buttons, &row_sizes);
        }
    }
}

pub(super) fn render_members(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    viewer_user_id: i64,
) {
    let family = match module
        .service
        .families()
        .get_family_for_user(viewer_user_id)
    {
        Ok(Some(family)) => family,
        Ok(None) => {
            let buttons = vec![btn("⬅️ Back", "wl:fam:back")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ You are not in a family.", &buttons, &row_sizes);
            return;
        }
        Err(e) => {
            crate::log_info(&format!("families::render_members error: {e}"));
            let buttons = vec![btn("⬅️ Back", "wl:fam:back")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load family.", &buttons, &row_sizes);
            return;
        }
    };

    let is_owner = family.owner_id == viewer_user_id;

    match module.service.families().get_family_members(family.id) {
        Ok(members) if members.len() <= 1 => {
            let mut buttons = Vec::new();
            if is_owner {
                buttons.push(btn("➕ Add member", "wl:fam:add"));
            }
            buttons.push(btn("⬅️ Back", "wl:fam:back"));
            let row_sizes: Vec<i32> = vec![1; buttons.len()];
            dest.buttons(
                ops,
                &format!("👥 {}\n\nYou are the only member so far.", family.name),
                &buttons,
                &row_sizes,
            );
        }
        Ok(members) => {
            let mut text = format!("👥 Members of {}:\n", family.name);
            for &member_user_id in &members {
                let name = ops.get_user_display_name(member_user_id);
                let role = if member_user_id == family.owner_id {
                    " 👑"
                } else {
                    ""
                };
                text.push_str(&format!("• {name}{role}\n"));
            }

            let mut buttons: Vec<ffi::CxxInlineButton> = if is_owner {
                members
                    .iter()
                    .filter(|&&member_user_id| member_user_id != viewer_user_id)
                    .map(|&member_user_id| {
                        let name = ops.get_user_display_name(member_user_id);
                        btn(
                            &format!("🗑 {name}"),
                            &format!("wl:fam:remove:{member_user_id}"),
                        )
                    })
                    .collect()
            } else {
                Vec::new()
            };
            if is_owner {
                buttons.push(btn("➕ Add member", "wl:fam:add"));
            }
            buttons.push(btn("⬅️ Back", "wl:fam:back"));
            let row_sizes: Vec<i32> = vec![1; buttons.len()];
            dest.buttons(ops, &text, &buttons, &row_sizes);
        }
        Err(e) => {
            crate::log_info(&format!("families::render_members error: {e}"));
            let buttons = vec![btn("⬅️ Back", "wl:fam:back")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load members.", &buttons, &row_sizes);
        }
    }
}

pub(super) fn render_browse(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    viewer_user_id: i64,
) {
    let family = match module
        .service
        .families()
        .get_family_for_user(viewer_user_id)
    {
        Ok(Some(family)) => family,
        Ok(None) => {
            let buttons = vec![btn("⬅️ Back", "wl:fam:back")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ You are not in a family.", &buttons, &row_sizes);
            return;
        }
        Err(e) => {
            crate::log_info(&format!("families::render_browse error: {e}"));
            let buttons = vec![btn("⬅️ Back", "wl:fam:back")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load family.", &buttons, &row_sizes);
            return;
        }
    };

    match module.service.families().get_family_members(family.id) {
        Ok(members) => {
            let others: Vec<_> = members
                .into_iter()
                .filter(|&member_user_id| member_user_id != viewer_user_id)
                .collect();
            if others.is_empty() {
                let buttons = vec![btn("⬅️ Back", "wl:fam:back")];
                let row_sizes = [1];
                dest.buttons(
                    ops,
                    "📋 No other family members to browse.",
                    &buttons,
                    &row_sizes,
                );
            } else {
                let mut buttons: Vec<ffi::CxxInlineButton> = others
                    .iter()
                    .map(|&member_user_id| {
                        let name = ops.get_user_display_name(member_user_id);
                        btn(
                            &format!("📋 {name}"),
                            &format!("wl:fam:lists:{member_user_id}"),
                        )
                    })
                    .collect();
                buttons.push(btn("⬅️ Back", "wl:fam:back"));
                let row_sizes: Vec<i32> = vec![1; buttons.len()];
                dest.buttons(
                    ops,
                    "📋 Whose lists would you like to see?",
                    &buttons,
                    &row_sizes,
                );
            }
        }
        Err(e) => {
            crate::log_info(&format!("families::render_browse error: {e}"));
            let buttons = vec![btn("⬅️ Back", "wl:fam:back")];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load members.", &buttons, &row_sizes);
        }
    }
}

pub(super) fn render_member_lists(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    viewer_user_id: i64,
    member_user_id: i64,
) {
    let member_name = ops.get_user_display_name(member_user_id);

    match module
        .service
        .browse_member_lists(viewer_user_id, member_user_id)
    {
        Ok(lists) if lists.is_empty() => {
            show_family_browse_message(
                ops,
                dest,
                &format!("📋 {member_name} has no public lists."),
            );
        }
        Ok(lists) => {
            let mut buttons: Vec<ffi::CxxInlineButton> = lists
                .iter()
                .map(|list| {
                    btn(
                        &list.name,
                        &format!("wl:fam:wishes:{member_user_id}:{}", list.id),
                    )
                })
                .collect();
            buttons.push(btn("⬅️ Back", "wl:fam:browse"));
            let row_sizes: Vec<i32> = vec![1; buttons.len()];
            dest.buttons(
                ops,
                &format!("📋 Public lists of {member_name}:"),
                &buttons,
                &row_sizes,
            );
        }
        Err(WishlistError::AccessDenied) => {
            show_family_browse_message(ops, dest, "❌ This member is no longer available.");
        }
        Err(e) => {
            crate::log_info(&format!("families::render_member_lists error: {e}"));
            show_family_browse_message(ops, dest, "❌ Failed to load lists.");
        }
    }
}

pub(super) fn render_member_wishes(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    viewer_user_id: i64,
    member_user_id: i64,
    list_id: i64,
) {
    match module.service.browse_member_wishes(viewer_user_id, list_id) {
        Ok(wishes) if wishes.is_empty() => {
            let buttons = vec![btn("⬅️ Back", &format!("wl:fam:lists:{member_user_id}"))];
            let row_sizes = [1];
            dest.buttons(ops, "📋 This list is empty.", &buttons, &row_sizes);
        }
        Ok(wishes) => {
            let mut buttons: Vec<ffi::CxxInlineButton> = wishes
                .iter()
                .enumerate()
                .map(|(idx, w)| {
                    let mark = match w.reserved_by {
                        Some(reserved_user_id) if reserved_user_id == viewer_user_id => " 🎁",
                        Some(_) => " 🔒",
                        None => "",
                    };
                    btn(
                        &format!("{}. {}{mark}", idx + 1, w.title),
                        &format!("wl:fam:wishdetail:{member_user_id}:{list_id}:{}", w.id),
                    )
                })
                .collect();
            buttons.push(btn("⬅️ Back", &format!("wl:fam:lists:{member_user_id}")));
            let row_sizes: Vec<i32> = vec![1; buttons.len()];
            dest.buttons(
                ops,
                "📋 Tap a wish to see details or reserve:",
                &buttons,
                &row_sizes,
            );
        }
        Err(WishlistError::AccessDenied) => {
            show_family_browse_message(ops, dest, "❌ This list is no longer available.");
        }
        Err(e) => {
            crate::log_info(&format!("families::render_member_wishes error: {e}"));
            show_family_browse_message(ops, dest, "❌ Failed to load wishes.");
        }
    }
}

pub(super) fn render_member_wish_detail(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    viewer_user_id: i64,
    member_user_id: i64,
    list_id: i64,
    wish_id: i64,
) {
    match module
        .service
        .view_member_wish(viewer_user_id, list_id, wish_id)
    {
        Ok(Some(wish)) => {
            let mut text = crate::wishes::format_wish_detail(&wish);

            let mut buttons = Vec::new();
            match wish.reserved_by {
                None => {
                    buttons.push(btn(
                        "🎁 Reserve",
                        &format!("wl:fam:reserve:{member_user_id}:{list_id}:{wish_id}"),
                    ));
                }
                Some(reserved_user_id) if reserved_user_id == viewer_user_id => {
                    text.push_str("\n\n🎁 You reserved this.");
                    buttons.push(btn(
                        "↩️ Unreserve",
                        &format!("wl:fam:unreserve:{member_user_id}:{list_id}:{wish_id}"),
                    ));
                }
                Some(_) => {
                    text.push_str("\n\n🔒 Reserved by someone else.");
                }
            }
            buttons.push(btn(
                "⬅️ Back",
                &format!("wl:fam:wishes:{member_user_id}:{list_id}"),
            ));
            let row_sizes: Vec<i32> = vec![1; buttons.len()];
            dest.buttons(ops, &text, &buttons, &row_sizes);
        }
        Ok(None) => {
            let buttons = vec![btn(
                "⬅️ Back",
                &format!("wl:fam:wishes:{member_user_id}:{list_id}"),
            )];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Wish not found.", &buttons, &row_sizes);
        }
        Err(WishlistError::AccessDenied) => {
            show_family_browse_message(ops, dest, "❌ This list is no longer available.");
        }
        Err(e) => {
            crate::log_info(&format!("families::render_member_wish_detail error: {e}"));
            let buttons = vec![btn(
                "⬅️ Back",
                &format!("wl:fam:wishes:{member_user_id}:{list_id}"),
            )];
            let row_sizes = [1];
            dest.buttons(ops, "❌ Failed to load wish.", &buttons, &row_sizes);
        }
    }
}
