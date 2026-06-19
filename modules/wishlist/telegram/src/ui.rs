use std::cell::Cell;

use crate::ffi;
use crate::session::SessionKey;

pub(crate) const MENU_PROMPT: &str = "🎁 Wishlist — choose an action:";
pub(crate) const MENU_ROW_SIZES: [i32; 2] = [3, 1];

pub(crate) fn btn(label: &str, callback_data: &str) -> ffi::CxxInlineButton {
    ffi::CxxInlineButton {
        label: label.to_string(),
        callback_data: callback_data.to_string(),
    }
}

pub(crate) fn menu_buttons() -> Vec<ffi::CxxInlineButton> {
    vec![
        btn("📋 Show", "wl:menu:show"),
        btn("➕ Add wish", "wl:menu:add"),
        btn("👪 Family", "wl:menu:family"),
        btn("❌ Exit", "wl:menu:exit"),
    ]
}

pub(crate) enum Target {
    Edit(i32),
    Send,
}

pub(crate) struct Dest {
    pub(crate) chat_id: i64,
    thread_id: i32,
    target: Target,
    last_sent_id: Cell<i32>,
}

impl Dest {
    pub(crate) fn edit(chat_id: i64, thread_id: i32, message_id: i32) -> Self {
        Self {
            chat_id,
            thread_id,
            target: Target::Edit(message_id),
            last_sent_id: Cell::new(0),
        }
    }

    pub(crate) fn send(chat_id: i64, thread_id: i32) -> Self {
        Self {
            chat_id,
            thread_id,
            target: Target::Send,
            last_sent_id: Cell::new(0),
        }
    }

    pub(crate) fn session_key(&self, user_id: i64) -> SessionKey {
        SessionKey::new(self.chat_id, self.thread_id, user_id)
    }

    pub(crate) fn buttons(
        &self,
        ops: &dyn crate::ModuleOpsLike,
        text: &str,
        buttons: &[ffi::CxxInlineButton],
        row_sizes: &[i32],
    ) {
        match self.target {
            Target::Edit(msg_id) => {
                ops.edit_with_inline_buttons(self.chat_id, msg_id, text, buttons, row_sizes);
            }
            Target::Send => {
                let msg_id = ops.send_with_inline_buttons(
                    self.chat_id,
                    self.thread_id,
                    text,
                    buttons,
                    row_sizes,
                );
                self.last_sent_id.set(msg_id);
            }
        }
    }

    pub(crate) fn text(&self, ops: &dyn crate::ModuleOpsLike, text: &str) {
        match self.target {
            Target::Edit(msg_id) => ops.edit_text(self.chat_id, msg_id, text),
            Target::Send => ops.send_text(self.chat_id, self.thread_id, text),
        }
    }

    pub(crate) fn menu(&self, ops: &dyn crate::ModuleOpsLike, text: &str) {
        self.buttons(ops, text, &menu_buttons(), &MENU_ROW_SIZES);
    }

    pub(crate) fn last_sent_id(&self) -> i32 {
        self.last_sent_id.get()
    }
}
