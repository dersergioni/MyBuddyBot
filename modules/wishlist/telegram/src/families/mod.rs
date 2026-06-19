mod actions;
mod callbacks;
mod render;

use crate::{Dest, WishlistModule};

pub(crate) use actions::{add_member, create, rename};
pub(crate) use callbacks::handle_callback;

pub(crate) const ACCEPT_PREFIX: &str = "wl:fam:accept:";
pub(crate) const DECLINE_PREFIX: &str = "wl:fam:decline:";

pub(crate) fn show(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    viewer_user_id: i64,
) {
    render::render_family(module, ops, dest, viewer_user_id);
}
