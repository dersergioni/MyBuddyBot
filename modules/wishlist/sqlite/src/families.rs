use rusqlite::{OptionalExtension, params};
use wishlist_core::WishlistError;
use wishlist_core::families::{FamilyRepository, FamilySummary, PendingInvitation};

use crate::FamilySqliteRepo;

impl FamilyRepository for FamilySqliteRepo {
    fn create_family(&self, owner_id: i64, name: &str) -> Result<i64, WishlistError> {
        let mut conn = self.conn.lock().unwrap();
        let tx = conn
            .transaction()
            .map_err(|e| WishlistError::Storage(format!("Failed to begin transaction: {e}")))?;
        tx.execute(
            "INSERT INTO families (owner_id, name) VALUES (?1, ?2)",
            params![owner_id, name],
        )
        .map_err(|e| WishlistError::Storage(format!("Failed to create family: {e}")))?;
        let family_id = tx.last_insert_rowid();
        tx.execute(
            "INSERT INTO family_members (family_id, user_id) VALUES (?1, ?2)",
            params![family_id, owner_id],
        )
        .map_err(|e| WishlistError::Storage(format!("Failed to add owner as member: {e}")))?;
        tx.commit()
            .map_err(|e| WishlistError::Storage(format!("Failed to commit: {e}")))?;
        Ok(family_id)
    }

    fn get_family_for_user(&self, user_id: i64) -> Result<Option<FamilySummary>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT f.id, f.owner_id, f.name FROM families f \
             JOIN family_members fm ON fm.family_id = f.id \
             WHERE fm.user_id = ?1 AND fm.accepted = 1",
            params![user_id],
            |row| {
                Ok(FamilySummary {
                    id: row.get::<_, i64>(0)?,
                    owner_id: row.get::<_, i64>(1)?,
                    name: row.get::<_, String>(2)?,
                })
            },
        )
        .optional()
        .map_err(|e| WishlistError::Storage(format!("Failed to query family: {e}")))
    }

    fn get_family_members(&self, family_id: i64) -> Result<Vec<i64>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn
            .prepare(
                "SELECT user_id FROM family_members \
                 WHERE family_id = ?1 AND accepted = 1 ORDER BY created_at",
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to query members: {e}")))?;
        let rows = stmt
            .query_map(params![family_id], |row| row.get::<_, i64>(0))
            .map_err(|e| WishlistError::Storage(format!("Failed to query members: {e}")))?;
        rows.collect::<Result<Vec<_>, _>>()
            .map_err(|e| WishlistError::Storage(format!("Failed to collect members: {e}")))
    }

    fn can_browse_family_member(
        &self,
        viewer_user_id: i64,
        member_user_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT COUNT(*) FROM family_members viewer_fm \
             JOIN family_members member_fm ON member_fm.family_id = viewer_fm.family_id \
             WHERE viewer_fm.user_id = ?1 AND viewer_fm.accepted = 1 \
             AND member_fm.user_id = ?2 AND member_fm.accepted = 1",
            params![viewer_user_id, member_user_id],
            |row| row.get::<_, i32>(0),
        )
        .map(|count| count > 0)
        .map_err(|e| {
            WishlistError::Storage(format!("Failed to validate family member access: {e}"))
        })
    }

    fn can_access_public_family_list(
        &self,
        viewer_user_id: i64,
        list_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT COUNT(*) FROM lists l \
             JOIN family_members viewer_fm ON viewer_fm.user_id = ?1 AND viewer_fm.accepted = 1 \
             JOIN family_members owner_fm \
               ON owner_fm.user_id = l.owner_id \
              AND owner_fm.accepted = 1 \
              AND owner_fm.family_id = viewer_fm.family_id \
             WHERE l.id = ?2 AND l.is_private = 0",
            params![viewer_user_id, list_id],
            |row| row.get::<_, i32>(0),
        )
        .map(|count| count > 0)
        .map_err(|e| WishlistError::Storage(format!("Failed to validate family list access: {e}")))
    }

    fn invite_member(&self, family_id: i64, member_user_id: i64) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "INSERT OR IGNORE INTO family_members (family_id, user_id, accepted) \
                 VALUES (?1, ?2, 0)",
                params![family_id, member_user_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to invite member: {e}")))?;
        if affected > 0 {
            return Ok(true);
        }
        let existing: Option<(i64, bool)> = conn
            .query_row(
                "SELECT family_id, accepted FROM family_members WHERE user_id = ?1",
                params![member_user_id],
                |row| Ok((row.get::<_, i64>(0)?, row.get::<_, bool>(1)?)),
            )
            .optional()
            .map_err(|e| {
                WishlistError::Storage(format!("Failed to check invitation status: {e}"))
            })?;
        match existing {
            Some((existing_family_id, false)) if existing_family_id == family_id => Ok(true),
            _ => Ok(false),
        }
    }

    fn remove_family_member(
        &self,
        family_id: i64,
        member_user_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "DELETE FROM family_members WHERE family_id = ?1 AND user_id = ?2",
                params![family_id, member_user_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to remove member: {e}")))?;
        Ok(affected > 0)
    }

    fn is_user_in_family(&self, user_id: i64) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT COUNT(*) FROM family_members WHERE user_id = ?1 AND accepted = 1",
            params![user_id],
            |row| row.get::<_, i32>(0),
        )
        .map(|count| count > 0)
        .map_err(|e| WishlistError::Storage(format!("Failed to check family membership: {e}")))
    }

    fn delete_family(&self, family_id: i64, owner_id: i64) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "DELETE FROM families WHERE id = ?1 AND owner_id = ?2",
                params![family_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to delete family: {e}")))?;
        Ok(affected > 0)
    }

    fn leave_family(&self, member_user_id: i64) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "DELETE FROM family_members WHERE user_id = ?1 \
                 AND family_id NOT IN (SELECT id FROM families WHERE owner_id = ?1)",
                params![member_user_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to leave family: {e}")))?;
        Ok(affected > 0)
    }

    fn rename_family(
        &self,
        family_id: i64,
        owner_id: i64,
        new_name: &str,
    ) -> Result<Option<FamilySummary>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE families SET name = ?1 WHERE id = ?2 AND owner_id = ?3",
                params![new_name, family_id, owner_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to rename family: {e}")))?;
        if affected > 0 {
            Ok(Some(FamilySummary {
                id: family_id,
                owner_id,
                name: new_name.to_string(),
            }))
        } else {
            Ok(None)
        }
    }

    fn get_pending_invitation(
        &self,
        invited_user_id: i64,
    ) -> Result<Option<PendingInvitation>, WishlistError> {
        let conn = self.conn.lock().unwrap();
        conn.query_row(
            "SELECT f.id, f.owner_id, f.name FROM families f \
             JOIN family_members fm ON fm.family_id = f.id \
             WHERE fm.user_id = ?1 AND fm.accepted = 0",
            params![invited_user_id],
            |row| {
                Ok(PendingInvitation {
                    family_id: row.get::<_, i64>(0)?,
                    owner_id: row.get::<_, i64>(1)?,
                    family_name: row.get::<_, String>(2)?,
                })
            },
        )
        .optional()
        .map_err(|e| WishlistError::Storage(format!("Failed to query invitation: {e}")))
    }

    fn accept_invitation(
        &self,
        family_id: i64,
        invited_user_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "UPDATE family_members SET accepted = 1 \
                 WHERE user_id = ?1 AND family_id = ?2 AND accepted = 0",
                params![invited_user_id, family_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to accept invitation: {e}")))?;
        Ok(affected > 0)
    }

    fn decline_invitation(
        &self,
        family_id: i64,
        invited_user_id: i64,
    ) -> Result<bool, WishlistError> {
        let conn = self.conn.lock().unwrap();
        let affected = conn
            .execute(
                "DELETE FROM family_members \
                 WHERE user_id = ?1 AND family_id = ?2 AND accepted = 0",
                params![invited_user_id, family_id],
            )
            .map_err(|e| WishlistError::Storage(format!("Failed to decline invitation: {e}")))?;
        Ok(affected > 0)
    }
}
