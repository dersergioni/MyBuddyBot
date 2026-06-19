use rusqlite::{OptionalExtension, params};
use wishlist_core::WishlistError;
use wishlist_core::wishes::{Wish, WishRepository};

use crate::WishSqliteRepo;

impl WishRepository for WishSqliteRepo {
    fn create_wish(&self, owner_id: i64, list_id: i64, title: &str) -> Result<i64, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "INSERT INTO wishes (list_id, title) \
                 SELECT id, ?2 FROM lists WHERE id = ?1 AND owner_id = ?3",
                params![list_id, title, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to add wish: {e}")))?;
        if affected == 0 {
            return Err(WishlistError::NotFound);
        }
        Ok(conn.last_insert_rowid())
    }

    fn get_wish(&self, list_id: i64, wish_id: i64) -> Result<Option<Wish>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT id, title, url, priority, notes, reserved_by \
             FROM wishes WHERE id = ?1 AND list_id = ?2",
            params![wish_id, list_id],
            |row| {
                Ok(Wish {
                    id: row.get(0)?,
                    title: row.get(1)?,
                    url: row.get(2)?,
                    priority: row.get(3)?,
                    notes: row.get(4)?,
                    reserved_by: row.get(5)?,
                })
            },
        )
        .optional()
        .map_err(|e| WishlistError::Storage(format!("Failed to query wish: {e}")))
    }

    fn get_owner_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Option<Wish>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT w.id, w.title, w.url, w.priority, w.notes, w.reserved_by \
             FROM wishes w \
             JOIN lists l ON l.id = w.list_id \
             WHERE w.id = ?1 AND w.list_id = ?2 AND l.owner_id = ?3",
            params![wish_id, list_id, owner_id],
            |row| {
                Ok(Wish {
                    id: row.get(0)?,
                    title: row.get(1)?,
                    url: row.get(2)?,
                    priority: row.get(3)?,
                    notes: row.get(4)?,
                    reserved_by: row.get(5)?,
                })
            },
        )
        .optional()
        .map_err(|e| WishlistError::Storage(format!("Failed to query owner wish: {e}")))
    }

    fn get_wish_owner(&self, list_id: i64, wish_id: i64) -> Result<Option<i64>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT l.owner_id \
             FROM wishes w \
             JOIN lists l ON l.id = w.list_id \
             WHERE w.id = ?1 AND w.list_id = ?2",
            params![wish_id, list_id],
            |row| row.get::<_, i64>(0),
        )
        .optional()
        .map_err(|e| WishlistError::Storage(format!("Failed to query wish owner: {e}")))
    }

    fn list_wishes(&self, list_id: i64) -> Result<Vec<Wish>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn
            .prepare(
                "SELECT id, title, url, priority, notes, reserved_by \
                 FROM wishes WHERE list_id = ?1 ORDER BY id",
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to query wishes: {e}")))?;
        let rows = stmt
            .query_map(params![list_id], |row| {
                Ok(Wish {
                    id: row.get(0)?,
                    title: row.get(1)?,
                    url: row.get(2)?,
                    priority: row.get(3)?,
                    notes: row.get(4)?,
                    reserved_by: row.get(5)?,
                })
            })
            .map_err(|e| WishlistError::Storage(format!("Failed to query wishes: {e}")))?;
        rows.collect::<Result<Vec<_>, _>>()
            .map_err(|e| WishlistError::Storage(format!("Failed to collect wishes: {e}")))
    }

    fn update_wish_title(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        title: &str,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE wishes SET title = ?1 WHERE id = ?2 AND list_id = ?3 \
                 AND list_id IN (SELECT id FROM lists WHERE owner_id = ?4)",
                params![title, wish_id, list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to update wish title: {e}")))?;
        Ok(affected > 0)
    }

    fn update_wish_url(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        url: Option<&str>,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE wishes SET url = ?1 WHERE id = ?2 AND list_id = ?3 \
                 AND list_id IN (SELECT id FROM lists WHERE owner_id = ?4)",
                params![url, wish_id, list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to update wish URL: {e}")))?;
        Ok(affected > 0)
    }

    fn update_wish_priority(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        priority: Option<i32>,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE wishes SET priority = ?1 WHERE id = ?2 AND list_id = ?3 \
                 AND list_id IN (SELECT id FROM lists WHERE owner_id = ?4)",
                params![priority, wish_id, list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to update priority: {e}")))?;
        Ok(affected > 0)
    }

    fn update_wish_notes(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        notes: Option<&str>,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE wishes SET notes = ?1 WHERE id = ?2 AND list_id = ?3 \
                 AND list_id IN (SELECT id FROM lists WHERE owner_id = ?4)",
                params![notes, wish_id, list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to update notes: {e}")))?;
        Ok(affected > 0)
    }

    fn delete_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "DELETE FROM wishes WHERE id = ?1 AND list_id = ?2 \
                 AND list_id IN (SELECT id FROM lists WHERE owner_id = ?3)",
                params![wish_id, list_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to delete wish: {e}")))?;
        Ok(affected > 0)
    }

    fn reserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE wishes SET reserved_by = ?1 \
                 WHERE id = ?2 AND list_id = ?3 AND reserved_by IS NULL",
                params![reserving_user_id, wish_id, list_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to reserve wish: {e}")))?;
        Ok(affected > 0)
    }

    fn unreserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE wishes SET reserved_by = NULL \
                 WHERE id = ?1 AND list_id = ?2 AND reserved_by = ?3",
                params![wish_id, list_id, reserving_user_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to unreserve wish: {e}")))?;
        Ok(affected > 0)
    }
}
