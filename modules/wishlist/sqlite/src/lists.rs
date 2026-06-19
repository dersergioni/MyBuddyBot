use rusqlite::{OptionalExtension, params};
use wishlist_core::WishlistError;
use wishlist_core::lists::{ListRepository, ListSummary, PublicListSummary};

use crate::ListSqliteRepo;

impl ListRepository for ListSqliteRepo {
    fn list_owner_lists(&self, owner_id: i64) -> Result<Vec<ListSummary>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT id, name, is_private FROM lists WHERE owner_id = ?1 ORDER BY id")
            .map_err(|e| WishlistError::Storage(format!("Failed to query lists: {e}")))?;
        let rows = stmt
            .query_map(params![owner_id], |row| {
                Ok(ListSummary {
                    id: row.get::<_, i64>(0)?,
                    name: row.get::<_, String>(1)?,
                    is_private: row.get::<_, i32>(2)? != 0,
                })
            })
            .map_err(|e| WishlistError::Storage(format!("Failed to query lists: {e}")))?;
        rows.collect::<Result<Vec<_>, _>>()
            .map_err(|e| WishlistError::Storage(format!("Failed to collect lists: {e}")))
    }

    fn list_public_lists(&self, owner_id: i64) -> Result<Vec<PublicListSummary>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn
            .prepare(
                "SELECT id, name FROM lists WHERE owner_id = ?1 AND is_private = 0 ORDER BY id",
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to query public lists: {e}")))?;
        let rows = stmt
            .query_map(params![owner_id], |row| {
                Ok(PublicListSummary {
                    id: row.get::<_, i64>(0)?,
                    name: row.get::<_, String>(1)?,
                })
            })
            .map_err(|e| WishlistError::Storage(format!("Failed to query public lists: {e}")))?;
        rows.collect::<Result<Vec<_>, _>>()
            .map_err(|e| WishlistError::Storage(format!("Failed to collect public lists: {e}")))
    }

    fn get_owner_list(
        &self,
        owner_id: i64,
        list_id: i64,
    ) -> Result<Option<ListSummary>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT id, name, is_private FROM lists WHERE id = ?1 AND owner_id = ?2",
            params![list_id, owner_id],
            |row| {
                Ok(ListSummary {
                    id: row.get::<_, i64>(0)?,
                    name: row.get::<_, String>(1)?,
                    is_private: row.get::<_, i32>(2)? != 0,
                })
            },
        )
        .optional()
        .map_err(|e| WishlistError::Storage(format!("Failed to query list: {e}")))
    }

    fn create_list(
        &self,
        owner_id: i64,
        name: &str,
        is_private: bool,
    ) -> Result<ListSummary, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.execute(
            "INSERT INTO lists (owner_id, name, is_private) VALUES (?1, ?2, ?3)",
            params![owner_id, name, is_private as i32],
        )
        .map_err(|e| WishlistError::Storage(format!("Failed to create list: {e}")))?;
        Ok(ListSummary {
            id: conn.last_insert_rowid(),
            name: name.to_string(),
            is_private,
        })
    }

    fn rename_list(
        &self,
        owner_id: i64,
        list_id: i64,
        new_name: &str,
    ) -> Result<Option<ListSummary>, WishlistError> {
        let affected = {
            let conn = self.conn.lock().unwrap();
            conn.execute(
                "UPDATE lists SET name = ?1 WHERE id = ?2 AND owner_id = ?3",
                params![new_name, list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to rename list: {e}")))?
        };
        if affected == 0 {
            return Ok(None);
        }
        self.get_owner_list(owner_id, list_id)
    }

    fn toggle_list_privacy(
        &self,
        owner_id: i64,
        list_id: i64,
    ) -> Result<Option<ListSummary>, WishlistError> {
        let affected = {
            let conn = self.conn.lock().unwrap();
            conn.execute(
                "UPDATE lists SET is_private = 1 - is_private WHERE id = ?1 AND owner_id = ?2",
                params![list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to toggle privacy: {e}")))?
        };
        if affected == 0 {
            return Ok(None);
        }
        self.get_owner_list(owner_id, list_id)
    }

    fn delete_list(&self, owner_id: i64, list_id: i64) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "DELETE FROM lists WHERE id = ?1 AND owner_id = ?2",
                params![list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to delete list: {e}")))?;
        Ok(affected > 0)
    }
}
