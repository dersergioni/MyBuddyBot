mod actions;
mod callbacks;
mod render;

use crate::{Dest, WishlistModule};

pub(crate) use actions::{create, rename};
pub(crate) use callbacks::handle_callback;

pub(crate) fn show(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
) {
    render::render_lists(module, ops, dest, owner_id);
}

pub(crate) fn show_for_adding(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
) {
    render::render_add_target_lists(module, ops, dest, owner_id);
}
