use std::collections::{HashMap, HashSet};
use std::sync::Mutex;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub(crate) struct SessionKey {
    pub(crate) chat_id: i64,
    pub(crate) thread_id: i32,
    pub(crate) user_id: i64,
}

impl SessionKey {
    pub(crate) fn new(chat_id: i64, thread_id: i32, user_id: i64) -> Self {
        Self {
            chat_id,
            thread_id,
            user_id,
        }
    }
}

pub(crate) enum InputState {
    ListName { is_private: bool },
    ListRename { list_id: i64 },
    WishTitle { list_id: i64 },
    WishCreateUrl { list_id: i64, wish_id: i64 },
    WishCreateNotes { list_id: i64, wish_id: i64 },
    WishEditTitle { list_id: i64, wish_id: i64 },
    WishEditUrl { list_id: i64, wish_id: i64 },
    WishEditNotes { list_id: i64, wish_id: i64 },
    FamilyName,
    FamilyMember { family_id: i64 },
    FamilyRename { family_id: i64 },
}

pub(crate) struct SessionState {
    input_state: Mutex<HashMap<SessionKey, InputState>>,
    active_sessions: Mutex<HashSet<SessionKey>>,
    active_message: Mutex<HashMap<SessionKey, i32>>,
    last_session_by_user: Mutex<HashMap<i64, SessionKey>>,
}

impl SessionState {
    pub(crate) fn new() -> Self {
        Self {
            input_state: Mutex::new(HashMap::new()),
            active_sessions: Mutex::new(HashSet::new()),
            active_message: Mutex::new(HashMap::new()),
            last_session_by_user: Mutex::new(HashMap::new()),
        }
    }

    pub(crate) fn is_active(&self, key: SessionKey) -> bool {
        self.active_sessions.lock().unwrap().contains(&key)
    }

    pub(crate) fn touch(&self, key: SessionKey) {
        self.active_sessions.lock().unwrap().insert(key);
        self.last_session_by_user
            .lock()
            .unwrap()
            .insert(key.user_id, key);
    }

    pub(crate) fn clear(&self, key: SessionKey) {
        self.input_state.lock().unwrap().remove(&key);
        self.active_sessions.lock().unwrap().remove(&key);
        self.active_message.lock().unwrap().remove(&key);

        let mut last_session_by_user = self.last_session_by_user.lock().unwrap();
        if last_session_by_user.get(&key.user_id).copied() == Some(key) {
            last_session_by_user.remove(&key.user_id);
        }
    }

    pub(crate) fn latest_active_session_for_user(&self, user_id: i64) -> Option<SessionKey> {
        let preferred = {
            self.last_session_by_user
                .lock()
                .unwrap()
                .get(&user_id)
                .copied()
        };
        if let Some(key) = preferred
            && self.is_active(key)
        {
            return Some(key);
        }

        let fallback = {
            let active_sessions = self.active_sessions.lock().unwrap();
            active_sessions
                .iter()
                .copied()
                .find(|key| key.user_id == user_id)
        };

        let mut last_session_by_user = self.last_session_by_user.lock().unwrap();
        if let Some(key) = fallback {
            last_session_by_user.insert(user_id, key);
        } else {
            last_session_by_user.remove(&user_id);
        }

        fallback
    }

    pub(crate) fn set_input_state(&self, key: SessionKey, state: InputState) {
        self.input_state.lock().unwrap().insert(key, state);
    }

    pub(crate) fn take_input_state(&self, key: SessionKey) -> Option<InputState> {
        self.input_state.lock().unwrap().remove(&key)
    }

    pub(crate) fn clear_input_state(&self, key: SessionKey) {
        self.input_state.lock().unwrap().remove(&key);
    }

    pub(crate) fn remember_active_message(&self, key: SessionKey, message_id: i32) {
        self.active_message.lock().unwrap().insert(key, message_id);
    }

    pub(crate) fn take_active_message(&self, key: SessionKey) -> Option<i32> {
        self.active_message.lock().unwrap().remove(&key)
    }

    pub(crate) fn is_session_active(&self, chat_id: i64, thread_id: i32, user_id: i64) -> bool {
        self.is_active(SessionKey::new(chat_id, thread_id, user_id))
    }

    pub(crate) fn deactivate_session(&self, chat_id: i64, thread_id: i32, user_id: i64) {
        self.clear(SessionKey::new(chat_id, thread_id, user_id));
    }
}

#[cfg(test)]
mod tests {
    use super::{InputState, SessionKey, SessionState};

    #[test]
    fn touch_and_clear_control_active_session_lifecycle() {
        let sessions = SessionState::new();
        let key = SessionKey::new(10, 20, 30);

        assert!(!sessions.is_active(key));
        sessions.touch(key);
        sessions.set_input_state(key, InputState::FamilyName);
        sessions.remember_active_message(key, 99);

        assert!(sessions.is_active(key));
        assert_eq!(
            sessions.latest_active_session_for_user(30),
            Some(SessionKey::new(10, 20, 30))
        );

        sessions.clear(key);

        assert!(!sessions.is_active(key));
        assert_eq!(sessions.latest_active_session_for_user(30), None);
        assert!(sessions.take_active_message(key).is_none());
        assert!(sessions.take_input_state(key).is_none());
    }

    #[test]
    fn latest_active_session_falls_back_when_preferred_session_was_cleared() {
        let sessions = SessionState::new();
        let old = SessionKey::new(10, 20, 30);
        let newer = SessionKey::new(11, 21, 30);

        sessions.touch(old);
        sessions.touch(newer);
        sessions.clear(newer);

        assert_eq!(sessions.latest_active_session_for_user(30), Some(old));
    }
}
