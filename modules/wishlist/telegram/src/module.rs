use crate::session::SessionState;
use wishlist_core::WishlistError;
use wishlist_core::service::WishlistService;
use wishlist_sqlite::WishlistSqlite;

pub struct WishlistModule {
    pub(crate) service: WishlistService,
    pub(crate) sessions: SessionState,
}

fn open_wishlist_db(db_path: &str) -> Result<WishlistSqlite, WishlistError> {
    WishlistSqlite::new(db_path)
}

pub(crate) fn create_wishlist_module(db_path: &str) -> Result<Box<WishlistModule>, WishlistError> {
    use wishlist_core::families::FamilyService;
    use wishlist_core::lists::ListService;
    use wishlist_core::wishes::WishService;

    crate::log_info(&format!("WishlistModule: opening DB at '{db_path}'"));
    let db = open_wishlist_db(db_path)?;
    crate::log_info("WishlistModule created");

    Ok(Box::new(WishlistModule {
        service: WishlistService::new(
            FamilyService::new(Box::new(db.family_repo())),
            ListService::new(Box::new(db.list_repo())),
            WishService::new(Box::new(db.wish_repo())),
        ),
        sessions: SessionState::new(),
    }))
}

pub(crate) fn get_trigger_keywords(_module: &WishlistModule) -> Vec<String> {
    vec!["wishlist".to_string(), "wish list".to_string()]
}

pub(crate) fn get_callback_prefix(_module: &WishlistModule) -> String {
    "wl:".to_string()
}

#[cfg(test)]
mod tests {
    use super::open_wishlist_db;
    use wishlist_core::WishlistError;

    #[test]
    fn open_wishlist_db_returns_error_for_missing_parent_directory() {
        let missing_dir = std::env::temp_dir()
            .join(format!(
                "wishlist-module-missing-parent-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .expect("system time should be after unix epoch")
                    .as_nanos()
            ))
            .join("wishlist.db");

        let result = open_wishlist_db(missing_dir.to_str().expect("path should be valid UTF-8"));

        assert!(
            result.is_err(),
            "db opening should fail when parent directory is missing"
        );

        let err = result.err().expect("result should contain an error");

        assert!(matches!(err, WishlistError::Storage(_)));
    }
}
