mod families;
mod lists;
mod wishes;

use rusqlite::Connection;
use std::sync::{Arc, Mutex};
use std::time::Duration;
use wishlist_core::WishlistError;

type SharedConnection = Arc<Mutex<Connection>>;

#[derive(Clone)]
pub struct WishlistSqlite {
    conn: SharedConnection,
}

fn open_connection(path: &str) -> Result<Connection, WishlistError> {
    let conn = Connection::open(path)
        .map_err(|e| WishlistError::Storage(format!("Failed to open DB: {e}")))?;
    conn.busy_timeout(Duration::from_secs(5))
        .map_err(|e| WishlistError::Storage(format!("Failed to configure busy timeout: {e}")))?;
    conn.execute_batch("PRAGMA foreign_keys = ON;")
        .map_err(|e| WishlistError::Storage(format!("Failed to enable foreign keys: {e}")))?;
    initialize_schema(&conn)?;
    Ok(conn)
}

fn initialize_schema(conn: &Connection) -> Result<(), WishlistError> {
    conn.execute_batch(
        "
        CREATE TABLE IF NOT EXISTS lists (
            id          INTEGER PRIMARY KEY,
            owner_id    INTEGER NOT NULL,
            name        TEXT    NOT NULL CHECK(length(name) > 0),
            is_private  INTEGER NOT NULL DEFAULT 0 CHECK(is_private IN (0, 1)),
            created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS wishes (
            id          INTEGER PRIMARY KEY,
            list_id     INTEGER NOT NULL REFERENCES lists(id) ON DELETE CASCADE,
            title       TEXT    NOT NULL CHECK(length(title) > 0),
            url         TEXT,
            priority    INTEGER CHECK(priority IS NULL OR priority BETWEEN 1 AND 3),
            notes       TEXT,
            reserved_by INTEGER,
            created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS families (
            id          INTEGER PRIMARY KEY,
            owner_id    INTEGER NOT NULL,
            name        TEXT    NOT NULL CHECK(length(name) > 0),
            created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS family_members (
            id            INTEGER PRIMARY KEY,
            family_id     INTEGER NOT NULL REFERENCES families(id) ON DELETE CASCADE,
            user_id       INTEGER NOT NULL UNIQUE,
            accepted      INTEGER NOT NULL DEFAULT 1 CHECK(accepted IN (0, 1)),
            created_at    TEXT    NOT NULL DEFAULT (datetime('now'))
        );

        CREATE UNIQUE INDEX IF NOT EXISTS idx_families_owner
            ON families(owner_id);
        CREATE INDEX IF NOT EXISTS idx_lists_owner
            ON lists(owner_id);
        CREATE INDEX IF NOT EXISTS idx_wishes_list
            ON wishes(list_id);
        CREATE INDEX IF NOT EXISTS idx_family_members_user
            ON family_members(user_id);
        ",
    )
    .map_err(|e| WishlistError::Storage(format!("Failed to create tables: {e}")))?;
    Ok(())
}

#[derive(Clone)]
pub struct FamilySqliteRepo {
    conn: SharedConnection,
}

#[derive(Clone)]
pub struct ListSqliteRepo {
    conn: SharedConnection,
}

#[derive(Clone)]
pub struct WishSqliteRepo {
    conn: SharedConnection,
}

impl WishlistSqlite {
    pub fn new(path: &str) -> Result<Self, WishlistError> {
        let conn = open_connection(path)?;
        Ok(Self {
            conn: Arc::new(Mutex::new(conn)),
        })
    }

    pub fn family_repo(&self) -> FamilySqliteRepo {
        FamilySqliteRepo {
            conn: Arc::clone(&self.conn),
        }
    }

    pub fn list_repo(&self) -> ListSqliteRepo {
        ListSqliteRepo {
            conn: Arc::clone(&self.conn),
        }
    }

    pub fn wish_repo(&self) -> WishSqliteRepo {
        WishSqliteRepo {
            conn: Arc::clone(&self.conn),
        }
    }
}
