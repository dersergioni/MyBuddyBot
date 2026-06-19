use crate::WishlistError;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Wish {
    pub id: i64,
    pub title: String,
    pub url: Option<String>,
    pub priority: Option<i32>,
    pub notes: Option<String>,
    pub reserved_by: Option<i64>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ReserveWishResult {
    Reserved {
        wish: Wish,
        notify_owner_id: Option<i64>,
    },
    AlreadyReserved,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum UnreserveWishResult {
    Unreserved(Wish),
    NotReservedByYou,
}

pub trait WishRepository {
    fn create_wish(&self, owner_id: i64, list_id: i64, title: &str) -> Result<i64, WishlistError>;
    fn get_wish(&self, list_id: i64, wish_id: i64) -> Result<Option<Wish>, WishlistError>;
    fn get_owner_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Option<Wish>, WishlistError>;
    fn get_wish_owner(&self, list_id: i64, wish_id: i64) -> Result<Option<i64>, WishlistError>;
    fn list_wishes(&self, list_id: i64) -> Result<Vec<Wish>, WishlistError>;
    fn update_wish_title(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        title: &str,
    ) -> Result<bool, WishlistError>;
    fn update_wish_url(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        url: Option<&str>,
    ) -> Result<bool, WishlistError>;
    fn update_wish_priority(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        priority: Option<i32>,
    ) -> Result<bool, WishlistError>;
    fn update_wish_notes(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        notes: Option<&str>,
    ) -> Result<bool, WishlistError>;
    fn delete_wish(&self, owner_id: i64, list_id: i64, wish_id: i64)
    -> Result<bool, WishlistError>;
    fn reserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError>;
    fn unreserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError>;
}

pub struct WishService {
    repo: Box<dyn WishRepository>,
}

impl WishService {
    pub fn new(repo: Box<dyn WishRepository>) -> Self {
        Self { repo }
    }

    pub fn list_wishes(&self, list_id: i64) -> Result<Vec<Wish>, WishlistError> {
        self.repo.list_wishes(list_id)
    }

    pub fn get_wish(&self, list_id: i64, wish_id: i64) -> Result<Option<Wish>, WishlistError> {
        self.repo.get_wish(list_id, wish_id)
    }

    pub fn get_owner_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Option<Wish>, WishlistError> {
        self.repo.get_owner_wish(owner_id, list_id, wish_id)
    }

    pub fn create_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        title: &str,
    ) -> Result<i64, WishlistError> {
        let trimmed_title = title.trim();
        if trimmed_title.is_empty() {
            return Err(WishlistError::Validation(
                "Wish title cannot be empty".to_string(),
            ));
        }

        self.repo.create_wish(owner_id, list_id, trimmed_title)
    }

    pub fn update_title(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        title: &str,
    ) -> Result<Wish, WishlistError> {
        let trimmed_title = title.trim();
        if trimmed_title.is_empty() {
            return Err(WishlistError::Validation(
                "Wish title cannot be empty".to_string(),
            ));
        }

        if !self
            .repo
            .update_wish_title(owner_id, list_id, wish_id, trimmed_title)?
        {
            return Err(WishlistError::NotFound);
        }

        self.repo
            .get_owner_wish(owner_id, list_id, wish_id)?
            .ok_or(WishlistError::NotFound)
    }

    pub fn update_url(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        url: Option<&str>,
    ) -> Result<Wish, WishlistError> {
        if !self.repo.update_wish_url(owner_id, list_id, wish_id, url)? {
            return Err(WishlistError::NotFound);
        }

        self.repo
            .get_owner_wish(owner_id, list_id, wish_id)?
            .ok_or(WishlistError::NotFound)
    }

    pub fn update_priority(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        priority: Option<i32>,
    ) -> Result<Wish, WishlistError> {
        if let Some(value) = priority
            && !(1..=3).contains(&value)
        {
            return Err(WishlistError::Validation(
                "Wish priority must be between 1 and 3".to_string(),
            ));
        }

        if !self
            .repo
            .update_wish_priority(owner_id, list_id, wish_id, priority)?
        {
            return Err(WishlistError::NotFound);
        }

        self.repo
            .get_owner_wish(owner_id, list_id, wish_id)?
            .ok_or(WishlistError::NotFound)
    }

    pub fn update_notes(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        notes: Option<&str>,
    ) -> Result<Wish, WishlistError> {
        if !self
            .repo
            .update_wish_notes(owner_id, list_id, wish_id, notes)?
        {
            return Err(WishlistError::NotFound);
        }

        self.repo
            .get_owner_wish(owner_id, list_id, wish_id)?
            .ok_or(WishlistError::NotFound)
    }

    pub fn delete_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<(), WishlistError> {
        if !self.repo.delete_wish(owner_id, list_id, wish_id)? {
            return Err(WishlistError::NotFound);
        }

        Ok(())
    }

    pub fn toggle_owner_reservation(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Wish, WishlistError> {
        let current = self
            .repo
            .get_owner_wish(owner_id, list_id, wish_id)?
            .ok_or(WishlistError::NotFound)?;

        match current.reserved_by {
            None => {
                if !self.repo.reserve_wish(owner_id, list_id, wish_id)? {
                    return Err(WishlistError::NotFound);
                }
                Ok(Wish {
                    reserved_by: Some(owner_id),
                    ..current
                })
            }
            Some(reserved_user_id) if reserved_user_id == owner_id => {
                if !self.repo.unreserve_wish(owner_id, list_id, wish_id)? {
                    return Err(WishlistError::NotFound);
                }
                Ok(Wish {
                    reserved_by: None,
                    ..current
                })
            }
            Some(_) => Ok(current),
        }
    }

    pub fn reserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<ReserveWishResult, WishlistError> {
        let owner_user_id = self
            .repo
            .get_wish_owner(list_id, wish_id)?
            .ok_or(WishlistError::NotFound)?;

        if !self
            .repo
            .reserve_wish(reserving_user_id, list_id, wish_id)?
        {
            if self.repo.get_wish(list_id, wish_id)?.is_some() {
                return Ok(ReserveWishResult::AlreadyReserved);
            }
            return Err(WishlistError::NotFound);
        }

        let wish = self
            .repo
            .get_wish(list_id, wish_id)?
            .ok_or(WishlistError::NotFound)?;

        let notify_owner_id = (owner_user_id != reserving_user_id).then_some(owner_user_id);
        Ok(ReserveWishResult::Reserved {
            wish,
            notify_owner_id,
        })
    }

    pub fn unreserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<UnreserveWishResult, WishlistError> {
        if !self
            .repo
            .unreserve_wish(reserving_user_id, list_id, wish_id)?
        {
            if self.repo.get_wish(list_id, wish_id)?.is_some() {
                return Ok(UnreserveWishResult::NotReservedByYou);
            }
            return Err(WishlistError::NotFound);
        }

        let wish = self
            .repo
            .get_wish(list_id, wish_id)?
            .ok_or(WishlistError::NotFound)?;

        Ok(UnreserveWishResult::Unreserved(wish))
    }
}
