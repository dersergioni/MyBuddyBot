use wishlist_core::families::CreateFamilyCheckResult;

use crate::{Dest, InputState, WishlistModule, btn};

use super::{actions, render};

fn owner_family_id_for_action(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    actor_user_id: i64,
    action_name: &str,
    not_owner_message: &str,
) -> Option<i64> {
    match module.service.families().get_family_for_user(actor_user_id) {
        Ok(Some(family)) if family.owner_id == actor_user_id => Some(family.id),
        Ok(Some(_)) => {
            dest.text(ops, not_owner_message);
            None
        }
        Ok(None) => {
            crate::log_info(&format!(
                "families::{action_name} callback: user_id={actor_user_id} is not in a family"
            ));
            dest.text(ops, "❌ You are not in a family.");
            None
        }
        Err(e) => {
            crate::log_info(&format!(
                "families::{action_name} callback: failed to load family: {e}"
            ));
            dest.text(ops, "❌ Failed to load family.");
            None
        }
    }
}

pub(crate) fn handle_callback(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    actor_user_id: i64,
    callback_data: &str,
    callback_query_id: &str,
) {
    match callback_data {
        "wl:fam:create" => {
            ops.answer_callback(callback_query_id, "");
            match module.service.families().check_create_family(actor_user_id) {
                Ok(CreateFamilyCheckResult::AlreadyInFamily) => {
                    dest.text(
                        ops,
                        "❌ You are already in a family. Leave it first to create a new one.",
                    );
                }
                Ok(CreateFamilyCheckResult::HasPendingInvitation) => {
                    dest.text(
                        ops,
                        "❌ You already have a pending family invitation. Accept or decline it first.",
                    );
                }
                Ok(CreateFamilyCheckResult::CanCreate) => {
                    module
                        .sessions
                        .set_input_state(dest.session_key(actor_user_id), InputState::FamilyName);
                    let buttons = vec![btn("❌ Cancel", "wl:menu:family")];
                    let row_sizes = [1];
                    dest.buttons(
                        ops,
                        "📝 Enter the name for your family:",
                        &buttons,
                        &row_sizes,
                    );
                }
                Err(e) => {
                    crate::log_info(&format!("families::create check error: {e}"));
                    dest.text(ops, "❌ Something went wrong.");
                }
            }
        }
        "wl:fam:members" => {
            ops.answer_callback(callback_query_id, "");
            render::render_members(module, ops, dest, actor_user_id);
        }
        "wl:fam:add" => {
            ops.answer_callback(callback_query_id, "");
            if let Some(family_id) = owner_family_id_for_action(
                module,
                ops,
                dest,
                actor_user_id,
                "add",
                "❌ Only the family owner can add members.",
            ) {
                module.sessions.set_input_state(
                    dest.session_key(actor_user_id),
                    InputState::FamilyMember { family_id },
                );
                let buttons = vec![btn("❌ Cancel", "wl:fam:back")];
                let row_sizes = [1];
                dest.buttons(
                    ops,
                    "📝 Enter the username of the person to add:\n\nExample: @john",
                    &buttons,
                    &row_sizes,
                );
            }
        }
        "wl:fam:browse" => {
            ops.answer_callback(callback_query_id, "");
            render::render_browse(module, ops, dest, actor_user_id);
        }
        "wl:fam:back" => {
            ops.answer_callback(callback_query_id, "");
            render::render_family(module, ops, dest, actor_user_id);
        }
        "wl:fam:rename" => {
            ops.answer_callback(callback_query_id, "");
            if let Some(family_id) = owner_family_id_for_action(
                module,
                ops,
                dest,
                actor_user_id,
                "rename",
                "❌ Only the family owner can rename the family.",
            ) {
                module.sessions.set_input_state(
                    dest.session_key(actor_user_id),
                    InputState::FamilyRename { family_id },
                );
                let buttons = vec![btn("❌ Cancel", "wl:fam:back")];
                let row_sizes = [1];
                dest.buttons(
                    ops,
                    "📝 Enter the new name for your family:",
                    &buttons,
                    &row_sizes,
                );
            }
        }
        _ if callback_data.starts_with(super::ACCEPT_PREFIX) => {
            ops.answer_callback(callback_query_id, "");
            if let Some(family_id_str) = callback_data.strip_prefix(super::ACCEPT_PREFIX)
                && let Ok(family_id) = family_id_str.parse::<i64>()
            {
                actions::handle_accept(module, ops, dest, actor_user_id, family_id);
            }
        }
        _ if callback_data.starts_with(super::DECLINE_PREFIX) => {
            ops.answer_callback(callback_query_id, "");
            if let Some(family_id_str) = callback_data.strip_prefix(super::DECLINE_PREFIX)
                && let Ok(family_id) = family_id_str.parse::<i64>()
            {
                actions::handle_decline(module, ops, dest, actor_user_id, family_id);
            }
        }
        "wl:fam:leave:confirm" => {
            ops.answer_callback(callback_query_id, "");
            let buttons = vec![
                btn("✅ Yes, leave", "wl:fam:leave"),
                btn("⬅️ Back", "wl:fam:back"),
            ];
            let row_sizes = [2];
            dest.buttons(
                ops,
                "Are you sure you want to leave the family?",
                &buttons,
                &row_sizes,
            );
        }
        "wl:fam:leave" => {
            ops.answer_callback(callback_query_id, "");
            actions::handle_leave(module, ops, dest, actor_user_id);
        }
        "wl:fam:delete" => {
            ops.answer_callback(callback_query_id, "");
            let buttons = vec![
                btn("✅ Yes, delete", "wl:fam:delete:yes"),
                btn("❌ Cancel", "wl:fam:back"),
            ];
            let row_sizes = [2];
            dest.buttons(
                ops,
                "⚠️ Delete your family? All members will be removed.",
                &buttons,
                &row_sizes,
            );
        }
        "wl:fam:delete:yes" => {
            ops.answer_callback(callback_query_id, "");
            actions::handle_delete_family(module, ops, dest, actor_user_id);
        }
        _ if callback_data.starts_with("wl:fam:remove:yes:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(member_user_id_str) = callback_data.strip_prefix("wl:fam:remove:yes:")
                && let Ok(member_user_id) = member_user_id_str.parse::<i64>()
            {
                actions::handle_remove_member(module, ops, dest, actor_user_id, member_user_id);
            }
        }
        _ if callback_data.starts_with("wl:fam:remove:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(member_user_id_str) = callback_data.strip_prefix("wl:fam:remove:")
                && let Ok(member_user_id) = member_user_id_str.parse::<i64>()
            {
                if member_user_id == actor_user_id {
                    dest.text(
                        ops,
                        "❌ You can't remove yourself. Delete the family instead.",
                    );
                    return;
                }
                let name = ops.get_user_display_name(member_user_id);
                let buttons = vec![
                    btn(
                        "✅ Yes, remove",
                        &format!("wl:fam:remove:yes:{member_user_id}"),
                    ),
                    btn("⬅️ Back", "wl:fam:members"),
                ];
                let row_sizes = [2];
                dest.buttons(
                    ops,
                    &format!("Remove {name} from the family?"),
                    &buttons,
                    &row_sizes,
                );
            }
        }
        _ if callback_data.starts_with("wl:fam:lists:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(member_user_id_str) = callback_data.strip_prefix("wl:fam:lists:")
                && let Ok(member_user_id) = member_user_id_str.parse::<i64>()
            {
                render::render_member_lists(module, ops, dest, actor_user_id, member_user_id);
            }
        }
        _ if callback_data.starts_with("wl:fam:reserve:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:fam:reserve:") {
                let parts: Vec<&str> = rest.splitn(3, ':').collect();
                if parts.len() == 3
                    && let (Ok(member_user_id), Ok(list_id), Ok(wish_id)) = (
                        parts[0].parse::<i64>(),
                        parts[1].parse::<i64>(),
                        parts[2].parse::<i64>(),
                    )
                {
                    actions::reserve_member_wish(
                        module,
                        ops,
                        dest,
                        actor_user_id,
                        member_user_id,
                        list_id,
                        wish_id,
                    );
                }
            }
        }
        _ if callback_data.starts_with("wl:fam:unreserve:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:fam:unreserve:") {
                let parts: Vec<&str> = rest.splitn(3, ':').collect();
                if parts.len() == 3
                    && let (Ok(member_user_id), Ok(list_id), Ok(wish_id)) = (
                        parts[0].parse::<i64>(),
                        parts[1].parse::<i64>(),
                        parts[2].parse::<i64>(),
                    )
                {
                    actions::unreserve_member_wish(
                        module,
                        ops,
                        dest,
                        actor_user_id,
                        member_user_id,
                        list_id,
                        wish_id,
                    );
                }
            }
        }
        _ if callback_data.starts_with("wl:fam:wishdetail:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:fam:wishdetail:") {
                let parts: Vec<&str> = rest.splitn(3, ':').collect();
                if parts.len() == 3
                    && let (Ok(member_user_id), Ok(list_id), Ok(wish_id)) = (
                        parts[0].parse::<i64>(),
                        parts[1].parse::<i64>(),
                        parts[2].parse::<i64>(),
                    )
                {
                    render::render_member_wish_detail(
                        module,
                        ops,
                        dest,
                        actor_user_id,
                        member_user_id,
                        list_id,
                        wish_id,
                    );
                }
            }
        }
        _ if callback_data.starts_with("wl:fam:wishes:") => {
            ops.answer_callback(callback_query_id, "");
            if let Some(rest) = callback_data.strip_prefix("wl:fam:wishes:")
                && let Some((member_str, list_str)) = rest.split_once(':')
                && let (Ok(member_user_id), Ok(list_id)) =
                    (member_str.parse::<i64>(), list_str.parse::<i64>())
            {
                render::render_member_wishes(
                    module,
                    ops,
                    dest,
                    actor_user_id,
                    member_user_id,
                    list_id,
                );
            }
        }
        _ => {
            ops.answer_callback(callback_query_id, "Unknown family action");
        }
    }
}
