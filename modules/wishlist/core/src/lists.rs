use crate::WishlistError;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ListSummary {
    pub id: i64,
    pub name: String,
    pub is_private: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PublicListSummary {
    pub id: i64,
    pub name: String,
}

pub trait ListRepository {
    fn list_owner_lists(&self, owner_id: i64) -> Result<Vec<ListSummary>, WishlistError>;
    fn list_public_lists(&self, owner_id: i64) -> Result<Vec<PublicListSummary>, WishlistError>;
    fn get_owner_list(
        &self,
        owner_id: i64,
        list_id: i64,
    ) -> Result<Option<ListSummary>, WishlistError>;
    fn create_list(
        &self,
        owner_id: i64,
        name: &str,
        is_private: bool,
    ) -> Result<ListSummary, WishlistError>;
    fn rename_list(
        &self,
        owner_id: i64,
        list_id: i64,
        new_name: &str,
    ) -> Result<Option<ListSummary>, WishlistError>;
    fn toggle_list_privacy(
        &self,
        owner_id: i64,
        list_id: i64,
    ) -> Result<Option<ListSummary>, WishlistError>;
    fn delete_list(&self, owner_id: i64, list_id: i64) -> Result<bool, WishlistError>;
}

pub struct ListService {
    repo: Box<dyn ListRepository>,
}

impl ListService {
    pub fn new(repo: Box<dyn ListRepository>) -> Self {
        Self { repo }
    }

    pub fn list_owner_lists(&self, owner_id: i64) -> Result<Vec<ListSummary>, WishlistError> {
        self.repo.list_owner_lists(owner_id)
    }

    pub fn list_public_lists(
        &self,
        owner_id: i64,
    ) -> Result<Vec<PublicListSummary>, WishlistError> {
        self.repo.list_public_lists(owner_id)
    }

    pub fn get_owner_list(
        &self,
        owner_id: i64,
        list_id: i64,
    ) -> Result<Option<ListSummary>, WishlistError> {
        self.repo.get_owner_list(owner_id, list_id)
    }

    pub fn create_list(
        &self,
        owner_id: i64,
        name: &str,
        is_private: bool,
    ) -> Result<ListSummary, WishlistError> {
        let trimmed_name = name.trim();
        if trimmed_name.is_empty() {
            return Err(WishlistError::Validation(
                "List name cannot be empty".to_string(),
            ));
        }

        self.repo.create_list(owner_id, trimmed_name, is_private)
    }

    pub fn rename_list(
        &self,
        owner_id: i64,
        list_id: i64,
        new_name: &str,
    ) -> Result<ListSummary, WishlistError> {
        let trimmed_name = new_name.trim();
        if trimmed_name.is_empty() {
            return Err(WishlistError::Validation(
                "List name cannot be empty".to_string(),
            ));
        }

        self.repo
            .rename_list(owner_id, list_id, trimmed_name)?
            .ok_or(WishlistError::NotFound)
    }

    pub fn toggle_list_privacy(
        &self,
        owner_id: i64,
        list_id: i64,
    ) -> Result<ListSummary, WishlistError> {
        self.repo
            .toggle_list_privacy(owner_id, list_id)?
            .ok_or(WishlistError::NotFound)
    }

    pub fn delete_list(&self, owner_id: i64, list_id: i64) -> Result<(), WishlistError> {
        if !self.repo.delete_list(owner_id, list_id)? {
            return Err(WishlistError::NotFound);
        }

        Ok(())
    }
}
