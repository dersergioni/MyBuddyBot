use crate::WishlistError;
use crate::families::FamilyService;
use crate::lists::{ListService, PublicListSummary};
use crate::wishes::{ReserveWishResult, UnreserveWishResult, Wish, WishService};

pub struct WishlistService {
    family_service: FamilyService,
    list_service: ListService,
    wish_service: WishService,
}

impl WishlistService {
    pub fn new(
        family_service: FamilyService,
        list_service: ListService,
        wish_service: WishService,
    ) -> Self {
        Self {
            family_service,
            list_service,
            wish_service,
        }
    }

    pub fn families(&self) -> &FamilyService {
        &self.family_service
    }

    pub fn lists(&self) -> &ListService {
        &self.list_service
    }

    pub fn wishes(&self) -> &WishService {
        &self.wish_service
    }

    pub fn browse_member_lists(
        &self,
        viewer_user_id: i64,
        member_user_id: i64,
    ) -> Result<Vec<PublicListSummary>, WishlistError> {
        if !self
            .family_service
            .can_browse_family_member(viewer_user_id, member_user_id)?
        {
            return Err(WishlistError::AccessDenied);
        }
        self.list_service.list_public_lists(member_user_id)
    }

    pub fn browse_member_wishes(
        &self,
        viewer_user_id: i64,
        list_id: i64,
    ) -> Result<Vec<Wish>, WishlistError> {
        if !self
            .family_service
            .can_access_public_family_list(viewer_user_id, list_id)?
        {
            return Err(WishlistError::AccessDenied);
        }
        self.wish_service.list_wishes(list_id)
    }

    pub fn view_member_wish(
        &self,
        viewer_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Option<Wish>, WishlistError> {
        if !self
            .family_service
            .can_access_public_family_list(viewer_user_id, list_id)?
        {
            return Err(WishlistError::AccessDenied);
        }
        self.wish_service.get_wish(list_id, wish_id)
    }

    pub fn reserve_family_wish(
        &self,
        actor_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<ReserveWishResult, WishlistError> {
        if !self
            .family_service
            .can_access_public_family_list(actor_user_id, list_id)?
        {
            return Err(WishlistError::AccessDenied);
        }
        self.wish_service
            .reserve_wish(actor_user_id, list_id, wish_id)
    }

    pub fn unreserve_family_wish(
        &self,
        actor_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<UnreserveWishResult, WishlistError> {
        if !self
            .family_service
            .can_access_public_family_list(actor_user_id, list_id)?
        {
            return Err(WishlistError::AccessDenied);
        }
        self.wish_service
            .unreserve_wish(actor_user_id, list_id, wish_id)
    }
}
