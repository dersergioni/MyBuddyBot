use crate::ffi;

pub(crate) trait ModuleOpsLike {
    fn send_text(&self, chat_id: i64, thread_id: i32, text: &str);
    fn send_with_inline_buttons(
        &self,
        chat_id: i64,
        thread_id: i32,
        text: &str,
        buttons: &[ffi::CxxInlineButton],
        row_sizes: &[i32],
    ) -> i32;
    fn edit_text(&self, chat_id: i64, message_id: i32, text: &str);
    fn edit_with_inline_buttons(
        &self,
        chat_id: i64,
        message_id: i32,
        text: &str,
        buttons: &[ffi::CxxInlineButton],
        row_sizes: &[i32],
    );
    fn answer_callback(&self, callback_query_id: &str, text: &str);
    fn delete_message(&self, chat_id: i64, message_id: i32);
    fn send_text_and_remove_reply_keyboard(&self, chat_id: i64, thread_id: i32, text: &str);
    fn lookup_user_by_username(&self, username: &str) -> i64;
    fn get_user_display_name(&self, user_id: i64) -> String;
}

impl ModuleOpsLike for ffi::ModuleOps {
    fn send_text(&self, chat_id: i64, thread_id: i32, text: &str) {
        ffi::ModuleOps::send_text(self, chat_id, thread_id, text);
    }

    fn send_with_inline_buttons(
        &self,
        chat_id: i64,
        thread_id: i32,
        text: &str,
        buttons: &[ffi::CxxInlineButton],
        row_sizes: &[i32],
    ) -> i32 {
        ffi::ModuleOps::send_with_inline_buttons(self, chat_id, thread_id, text, buttons, row_sizes)
    }

    fn edit_text(&self, chat_id: i64, message_id: i32, text: &str) {
        ffi::ModuleOps::edit_text(self, chat_id, message_id, text);
    }

    fn edit_with_inline_buttons(
        &self,
        chat_id: i64,
        message_id: i32,
        text: &str,
        buttons: &[ffi::CxxInlineButton],
        row_sizes: &[i32],
    ) {
        ffi::ModuleOps::edit_with_inline_buttons(
            self, chat_id, message_id, text, buttons, row_sizes,
        );
    }

    fn answer_callback(&self, callback_query_id: &str, text: &str) {
        ffi::ModuleOps::answer_callback(self, callback_query_id, text);
    }

    fn delete_message(&self, chat_id: i64, message_id: i32) {
        ffi::ModuleOps::delete_message(self, chat_id, message_id);
    }

    fn send_text_and_remove_reply_keyboard(&self, chat_id: i64, thread_id: i32, text: &str) {
        ffi::ModuleOps::send_text_and_remove_reply_keyboard(self, chat_id, thread_id, text);
    }

    fn lookup_user_by_username(&self, username: &str) -> i64 {
        ffi::ModuleOps::lookup_user_by_username(self, username)
    }

    fn get_user_display_name(&self, user_id: i64) -> String {
        ffi::ModuleOps::get_user_display_name(self, user_id)
    }
}

pub(crate) fn log_info(message: &str) {
    #[cfg(not(test))]
    {
        ffi::log_info(message);
    }
    #[cfg(test)]
    {
        let _ = message;
    }
}

pub(crate) fn log_debug(message: &str) {
    #[cfg(not(test))]
    {
        ffi::log_debug(message);
    }
    #[cfg(test)]
    {
        let _ = message;
    }
}

#[cfg(test)]
#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct RecordedButton {
    pub(crate) label: String,
    pub(crate) callback_data: String,
}

#[cfg(test)]
impl From<&ffi::CxxInlineButton> for RecordedButton {
    fn from(button: &ffi::CxxInlineButton) -> Self {
        Self {
            label: button.label.clone(),
            callback_data: button.callback_data.clone(),
        }
    }
}

#[cfg(test)]
#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) enum RecordedOp {
    SendText {
        chat_id: i64,
        thread_id: i32,
        text: String,
    },
    SendWithInlineButtons {
        chat_id: i64,
        thread_id: i32,
        text: String,
        buttons: Vec<RecordedButton>,
        row_sizes: Vec<i32>,
        message_id: i32,
    },
    EditText {
        chat_id: i64,
        message_id: i32,
        text: String,
    },
    EditWithInlineButtons {
        chat_id: i64,
        message_id: i32,
        text: String,
        buttons: Vec<RecordedButton>,
        row_sizes: Vec<i32>,
    },
    AnswerCallback {
        callback_query_id: String,
        text: String,
    },
    DeleteMessage {
        chat_id: i64,
        message_id: i32,
    },
    SendTextAndRemoveReplyKeyboard {
        chat_id: i64,
        thread_id: i32,
        text: String,
    },
}

#[cfg(test)]
pub(crate) struct FakeModuleOps {
    next_message_id: std::cell::Cell<i32>,
    operations: std::cell::RefCell<Vec<RecordedOp>>,
    username_lookup: std::cell::RefCell<std::collections::HashMap<String, i64>>,
    display_names: std::cell::RefCell<std::collections::HashMap<i64, String>>,
}

#[cfg(test)]
impl Default for FakeModuleOps {
    fn default() -> Self {
        Self {
            next_message_id: std::cell::Cell::new(1),
            operations: std::cell::RefCell::new(Vec::new()),
            username_lookup: std::cell::RefCell::new(std::collections::HashMap::new()),
            display_names: std::cell::RefCell::new(std::collections::HashMap::new()),
        }
    }
}

#[cfg(test)]
#[allow(dead_code)]
impl FakeModuleOps {
    pub(crate) fn set_user_lookup(&self, username: &str, user_id: i64) {
        self.username_lookup
            .borrow_mut()
            .insert(username.to_string(), user_id);
    }

    pub(crate) fn set_display_name(&self, user_id: i64, display_name: &str) {
        self.display_names
            .borrow_mut()
            .insert(user_id, display_name.to_string());
    }

    pub(crate) fn operations(&self) -> Vec<RecordedOp> {
        self.operations.borrow().clone()
    }

    pub(crate) fn take_operations(&self) -> Vec<RecordedOp> {
        std::mem::take(&mut *self.operations.borrow_mut())
    }

    fn allocate_message_id(&self) -> i32 {
        let id = self.next_message_id.get();
        self.next_message_id.set(id + 1);
        id
    }
}

#[cfg(test)]
impl ModuleOpsLike for FakeModuleOps {
    fn send_text(&self, chat_id: i64, thread_id: i32, text: &str) {
        self.operations.borrow_mut().push(RecordedOp::SendText {
            chat_id,
            thread_id,
            text: text.to_string(),
        });
    }

    fn send_with_inline_buttons(
        &self,
        chat_id: i64,
        thread_id: i32,
        text: &str,
        buttons: &[ffi::CxxInlineButton],
        row_sizes: &[i32],
    ) -> i32 {
        let message_id = self.allocate_message_id();
        self.operations
            .borrow_mut()
            .push(RecordedOp::SendWithInlineButtons {
                chat_id,
                thread_id,
                text: text.to_string(),
                buttons: buttons.iter().map(RecordedButton::from).collect(),
                row_sizes: row_sizes.to_vec(),
                message_id,
            });
        message_id
    }

    fn edit_text(&self, chat_id: i64, message_id: i32, text: &str) {
        self.operations.borrow_mut().push(RecordedOp::EditText {
            chat_id,
            message_id,
            text: text.to_string(),
        });
    }

    fn edit_with_inline_buttons(
        &self,
        chat_id: i64,
        message_id: i32,
        text: &str,
        buttons: &[ffi::CxxInlineButton],
        row_sizes: &[i32],
    ) {
        self.operations
            .borrow_mut()
            .push(RecordedOp::EditWithInlineButtons {
                chat_id,
                message_id,
                text: text.to_string(),
                buttons: buttons.iter().map(RecordedButton::from).collect(),
                row_sizes: row_sizes.to_vec(),
            });
    }

    fn answer_callback(&self, callback_query_id: &str, text: &str) {
        self.operations
            .borrow_mut()
            .push(RecordedOp::AnswerCallback {
                callback_query_id: callback_query_id.to_string(),
                text: text.to_string(),
            });
    }

    fn delete_message(&self, chat_id: i64, message_id: i32) {
        self.operations
            .borrow_mut()
            .push(RecordedOp::DeleteMessage {
                chat_id,
                message_id,
            });
    }

    fn send_text_and_remove_reply_keyboard(&self, chat_id: i64, thread_id: i32, text: &str) {
        self.operations
            .borrow_mut()
            .push(RecordedOp::SendTextAndRemoveReplyKeyboard {
                chat_id,
                thread_id,
                text: text.to_string(),
            });
    }

    fn lookup_user_by_username(&self, username: &str) -> i64 {
        self.username_lookup
            .borrow()
            .get(username)
            .copied()
            .unwrap_or_default()
    }

    fn get_user_display_name(&self, user_id: i64) -> String {
        self.display_names
            .borrow()
            .get(&user_id)
            .cloned()
            .unwrap_or_else(|| format!("User {user_id}"))
    }
}
