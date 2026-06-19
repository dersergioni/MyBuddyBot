mod actions;
mod callbacks;
mod render;

use crate::{Dest, WishlistModule};

pub(crate) use actions::{create, create_notes, create_url, edit_notes, edit_title, edit_url};
pub(crate) use callbacks::handle_callback;
pub(crate) use render::format_wish_detail;

pub(crate) fn show(
    module: &WishlistModule,
    ops: &dyn crate::ModuleOpsLike,
    dest: &Dest,
    owner_id: i64,
    list_id: i64,
) {
    render::render_list(module, ops, dest, owner_id, list_id);
}
