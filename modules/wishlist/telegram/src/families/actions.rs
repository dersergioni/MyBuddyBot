use wishlist_core::WishlistError;
use wishlist_core::families::{
    AcceptInvitationResult, CreateFamilyResult, DeclineInvitationResult, DeleteFamilyResult,
    InviteMemberResult, LeaveFamilyResult, RemoveMemberResult,
};
use wishlist_core::wishes::{ReserveWishResult, UnreserveWishResult};

use crate::{Dest, WishlistModule, btn, send_user_notification};

use super::render;

const RESERVED_WISH_NOTIFICATION: &str =
    "🎉 Wow! Someone just reserved one of your wishlist gifts.\n\nA little surprise is on the way.";

pub(crate) fn create(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    name: &str,
) -> bool {
    match module.service.families().create_family(owner_id, name) {
        Ok(CreateFamilyResult::Created(family)) => {
            dest.menu(ops, &format!("✅ Family \"{}\" created!", family.name));
            true
        }
        Ok(CreateFamilyResult::AlreadyInFamily) => {
            dest.menu(
                ops,
                "❌ You are already in a family. Leave it first to create a new one.",
            );
            true
        }
        Ok(CreateFamilyResult::HasPendingInvitation) => {
            dest.menu(
                ops,
                "❌ You already have a pending family invitation. Accept or decline it first.",
            );
            true
        }
        Err(WishlistError::Validation(_)) => {
            let buttons = vec![btn("❌ Cancel", "wl:menu:family")];
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
            crate::log_info(&format!("families::create error: {e}"));
            let buttons = vec![btn("❌ Cancel", "wl:menu:family")];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to create family. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(crate) fn add_member(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    family_id: i64,
    text: &str,
) -> bool {
    let cancel_buttons = vec![btn("❌ Cancel", "wl:fam:back")];
    let cancel_row_sizes = [1];

    let username = text.trim().trim_start_matches('@');
    if username.is_empty() {
        dest.buttons(
            ops,
            "❌ Please enter a username (e.g. @john). Try again:",
            &cancel_buttons,
            &cancel_row_sizes,
        );
        return false;
    }

    let member_user_id = ops.lookup_user_by_username(username);
    if member_user_id == 0 {
        dest.buttons(
            ops,
            &format!(
                "❌ User @{username} hasn't interacted with the bot yet.\n\
                 Ask them to send any message to the bot first.\n\n\
                 Try another username:"
            ),
            &cancel_buttons,
            &cancel_row_sizes,
        );
        return false;
    }

    match module
        .service
        .families()
        .invite_member_by_owner(owner_id, family_id, member_user_id)
    {
        Ok(InviteMemberResult::Invited { family_name }) => {
            let display = ops.get_user_display_name(member_user_id);
            dest.menu(ops, &format!("✅ Invitation sent to {display}!"));

            let owner_name = ops.get_user_display_name(owner_id);
            let buttons = vec![
                btn("✅ Accept", &format!("wl:fam:accept:{family_id}")),
                btn("❌ Decline", &format!("wl:fam:decline:{family_id}")),
            ];
            let row_sizes = [2];
            ops.send_with_inline_buttons(
                member_user_id,
                0,
                &format!("👪 {owner_name} invited you to family \"{family_name}\""),
                &buttons,
                &row_sizes,
            );
            true
        }
        Ok(InviteMemberResult::AlreadyInFamily) => {
            dest.buttons(
                ops,
                &format!("❌ @{username} is already in a family. Try another username:"),
                &cancel_buttons,
                &cancel_row_sizes,
            );
            false
        }
        Ok(InviteMemberResult::HasPendingInvitation) => {
            dest.buttons(
                ops,
                &format!(
                    "❌ @{username} already has a pending family invitation. Try another username:"
                ),
                &cancel_buttons,
                &cancel_row_sizes,
            );
            false
        }
        Ok(InviteMemberResult::NotOwner) => {
            dest.menu(ops, "❌ Only the family owner can add members.");
            true
        }
        Err(e) => {
            crate::log_info(&format!("families::add_member error: {e}"));
            dest.menu(ops, "❌ Failed to send invitation.");
            true
        }
    }
}

pub(crate) fn rename(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    family_id: i64,
    text: &str,
) -> bool {
    match module
        .service
        .families()
        .rename_family(family_id, owner_id, text)
    {
        Ok(family) => {
            dest.menu(ops, &format!("✅ Family renamed to \"{}\"!", family.name));
            true
        }
        Err(WishlistError::Validation(_)) => {
            let buttons = vec![btn("❌ Cancel", "wl:fam:back")];
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
            dest.menu(ops, "❌ Could not rename. Are you the owner?");
            true
        }
        Err(e) => {
            crate::log_info(&format!("families::rename error: {e}"));
            let buttons = vec![btn("❌ Cancel", "wl:fam:back")];
            let row_sizes = [1];
            dest.buttons(
                ops,
                "❌ Failed to rename family. Try again:",
                &buttons,
                &row_sizes,
            );
            false
        }
    }
}

pub(super) fn handle_remove_member(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    member_user_id: i64,
) {
    crate::log_info(&format!(
        "Family: removing member user_id={member_user_id} by owner={owner_id}"
    ));
    match module
        .service
        .families()
        .remove_member_by_owner(owner_id, member_user_id)
    {
        Ok(RemoveMemberResult::Removed) => {
            render::render_members(module, ops, dest, owner_id);
        }
        Ok(RemoveMemberResult::MemberNotFound) => {
            dest.text(ops, "❌ Member not found.");
        }
        Ok(RemoveMemberResult::NotOwner) => {
            dest.text(ops, "❌ Only the owner can remove members.");
        }
        Ok(RemoveMemberResult::CannotRemoveSelf) => {
            dest.text(
                ops,
                "❌ You can't remove yourself. Delete the family instead.",
            );
        }
        Err(e) => {
            crate::log_info(&format!("families::remove_member error: {e}"));
            dest.text(ops, "❌ Failed to remove member.");
        }
    }
}

pub(super) fn handle_leave(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    member_user_id: i64,
) {
    crate::log_info(&format!("Family: user_id={member_user_id} leaving family"));
    match module
        .service
        .families()
        .leave_family_checked(member_user_id)
    {
        Ok(LeaveFamilyResult::Left) => {
            render::render_family(module, ops, dest, member_user_id);
        }
        Ok(LeaveFamilyResult::OwnerCannotLeave) => {
            dest.text(ops, "❌ Could not leave family. Are you the owner?");
        }
        Ok(LeaveFamilyResult::NotInFamily) => {
            dest.text(ops, "❌ You are not in a family.");
        }
        Err(e) => {
            crate::log_info(&format!("families::leave error: {e}"));
            dest.text(ops, "❌ Failed to leave family.");
        }
    }
}

pub(super) fn handle_accept(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    invited_user_id: i64,
    family_id: i64,
) {
    match module
        .service
        .families()
        .accept_invitation_checked(invited_user_id, family_id)
    {
        Ok(AcceptInvitationResult::Accepted) => {
            dest.text(ops, "✅ You joined the family!");
        }
        Ok(AcceptInvitationResult::AlreadyInFamily) => {
            dest.text(ops, "ℹ️ You are already in a family!")
        }
        Ok(AcceptInvitationResult::InvitationNotFound) => {
            dest.text(ops, "❌ No pending invitation found.")
        }
        Err(e) => {
            crate::log_info(&format!("families::accept error: {e}"));
            dest.text(ops, "❌ Failed to accept invitation.");
        }
    }
}

pub(super) fn handle_decline(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    invited_user_id: i64,
    family_id: i64,
) {
    crate::log_info(&format!(
        "Family: user_id={invited_user_id} declining invitation"
    ));
    match module
        .service
        .families()
        .decline_invitation_checked(invited_user_id, family_id)
    {
        Ok(DeclineInvitationResult::Declined) => {
            dest.text(ops, "✅ Invitation declined.");
        }
        Ok(DeclineInvitationResult::AlreadyInFamily) => {
            dest.text(ops, "ℹ️ You are already in a family!")
        }
        Ok(DeclineInvitationResult::InvitationNotFound) => {
            dest.text(ops, "❌ No pending invitation found.")
        }
        Err(e) => {
            crate::log_info(&format!("families::decline error: {e}"));
            dest.text(ops, "❌ Failed to decline invitation.");
        }
    }
}

pub(super) fn handle_delete_family(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
) {
    crate::log_info(&format!("Family: user_id={owner_id} deleting family"));
    match module.service.families().delete_owned_family(owner_id) {
        Ok(DeleteFamilyResult::Deleted) => {
            render::render_family(module, ops, dest, owner_id);
        }
        Ok(DeleteFamilyResult::NotOwner) => {
            dest.text(ops, "❌ You are not a family owner.");
        }
        Err(e) => {
            crate::log_info(&format!("families::delete error: {e}"));
            dest.text(ops, "❌ Failed to delete family.");
        }
    }
}

pub(super) fn reserve_member_wish(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    actor_user_id: i64,
    member_user_id: i64,
    list_id: i64,
    wish_id: i64,
) {
    match module
        .service
        .reserve_family_wish(actor_user_id, list_id, wish_id)
    {
        Ok(ReserveWishResult::Reserved {
            notify_owner_id, ..
        }) => {
            render::render_member_wish_detail(
                module,
                ops,
                dest,
                actor_user_id,
                member_user_id,
                list_id,
                wish_id,
            );
            if let Some(owner_id) = notify_owner_id {
                send_user_notification(module, ops, owner_id, RESERVED_WISH_NOTIFICATION);
            }
        }
        Ok(ReserveWishResult::AlreadyReserved) => {
            dest.text(ops, "❌ Already reserved by someone else.");
        }
        Err(WishlistError::AccessDenied) => {
            render::show_family_browse_message(ops, dest, "❌ This list is no longer available.");
        }
        Err(WishlistError::NotFound) => {
            dest.text(ops, "❌ Wish not found.");
        }
        Err(e) => {
            crate::log_info(&format!("families::reserve error: {e}"));
            dest.text(ops, "❌ Failed to reserve.");
        }
    }
}

pub(super) fn unreserve_member_wish(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    actor_user_id: i64,
    member_user_id: i64,
    list_id: i64,
    wish_id: i64,
) {
    match module
        .service
        .unreserve_family_wish(actor_user_id, list_id, wish_id)
    {
        Ok(UnreserveWishResult::Unreserved(_)) => {
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
        Ok(UnreserveWishResult::NotReservedByYou) => {
            dest.text(ops, "❌ You didn't reserve this.");
        }
        Err(WishlistError::AccessDenied) => {
            render::show_family_browse_message(ops, dest, "❌ This list is no longer available.");
        }
        Err(WishlistError::NotFound) => {
            dest.text(ops, "❌ Wish not found.");
        }
        Err(e) => {
            crate::log_info(&format!("families::unreserve error: {e}"));
            dest.text(ops, "❌ Failed to unreserve.");
        }
    }
}
