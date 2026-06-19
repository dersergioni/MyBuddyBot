#![allow(dead_code)]

use std::fs;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{SystemTime, UNIX_EPOCH};

use rusqlite::Connection;
use wishlist_core::WishlistError;
use wishlist_core::families::FamilyRepository;
use wishlist_core::lists::ListRepository;
use wishlist_core::wishes::WishRepository;
use wishlist_sqlite::WishlistSqlite;

static NEXT_DB_ID: AtomicU64 = AtomicU64::new(0);

fn unique_db_path() -> PathBuf {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("system time should be after unix epoch")
        .as_nanos();
    let sequence = NEXT_DB_ID.fetch_add(1, Ordering::Relaxed);
    std::env::temp_dir().join(format!(
        "wishlist-sqlite-test-{}-{nanos}-{sequence}.db",
        std::process::id()
    ))
}

pub struct TestDb {
    path: PathBuf,
}

impl TestDb {
    pub fn new() -> Self {
        Self {
            path: unique_db_path(),
        }
    }

    pub fn repo(&self) -> WishlistSqlite {
        WishlistSqlite::new(self.path.to_str().expect("db path should be valid UTF-8"))
            .expect("sqlite adapter should initialize")
    }

    pub fn raw_connection(&self) -> Connection {
        Connection::open(&self.path).expect("raw sqlite connection should open")
    }
}

impl Drop for TestDb {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.path);
    }
}

pub fn create_family(repo: &WishlistSqlite, owner_id: i64, name: &str) -> i64 {
    let family_repo = repo.family_repo();
    FamilyRepository::create_family(&family_repo, owner_id, name).expect("family should be created")
}

pub fn invite_member(repo: &WishlistSqlite, family_id: i64, member_user_id: i64) {
    let family_repo = repo.family_repo();
    assert!(
        FamilyRepository::invite_member(&family_repo, family_id, member_user_id)
            .expect("invitation should succeed"),
        "invitation should be recorded"
    );
}

pub fn accept_invitation(repo: &WishlistSqlite, invited_user_id: i64, family_id: i64) {
    let family_repo = repo.family_repo();
    assert!(
        FamilyRepository::accept_invitation(&family_repo, family_id, invited_user_id)
            .expect("acceptance should succeed"),
        "pending invitation should be accepted"
    );
}

pub fn create_list(
    repo: &WishlistSqlite,
    owner_id: i64,
    name: &str,
    is_private: bool,
) -> wishlist_core::lists::ListSummary {
    let list_repo = repo.list_repo();
    ListRepository::create_list(&list_repo, owner_id, name, is_private)
        .expect("list should be created")
}

pub fn create_wish(repo: &WishlistSqlite, owner_id: i64, list_id: i64, title: &str) -> i64 {
    let wish_repo = repo.wish_repo();
    WishRepository::create_wish(&wish_repo, owner_id, list_id, title)
        .expect("wish creation should succeed")
}

pub fn assert_storage_error(result: Result<(), WishlistError>, expected_fragment: &str) {
    match result {
        Err(WishlistError::Storage(message)) => {
            assert!(
                message.contains(expected_fragment),
                "expected storage error containing {expected_fragment:?}, got {message:?}"
            );
        }
        other => panic!("expected storage error, got {other:?}"),
    }
}
