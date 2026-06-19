mod support;

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::rc::Rc;

use support::{public_list, wish};
use wishlist_core::WishlistError;
use wishlist_core::families::{FamilyRepository, FamilyService};
use wishlist_core::lists::{ListRepository, ListService};
use wishlist_core::service::WishlistService;
use wishlist_core::wishes::{ReserveWishResult, UnreserveWishResult, WishRepository, WishService};

#[derive(Debug, Default)]
struct FamilyAccessRepo {
    can_browse: RefCell<HashMap<(i64, i64), bool>>,
    can_access_list: RefCell<HashMap<(i64, i64), bool>>,
}

impl FamilyRepository for FamilyAccessRepo {
    fn create_family(&self, _owner_id: i64, _name: &str) -> Result<i64, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn get_family_for_user(
        &self,
        _user_id: i64,
    ) -> Result<Option<wishlist_core::families::FamilySummary>, WishlistError> {
        Ok(None)
    }

    fn get_family_members(&self, _family_id: i64) -> Result<Vec<i64>, WishlistError> {
        Ok(Vec::new())
    }

    fn can_browse_family_member(
        &self,
        viewer_user_id: i64,
        member_user_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .can_browse
            .borrow()
            .get(&(viewer_user_id, member_user_id))
            .unwrap_or(&false))
    }

    fn can_access_public_family_list(
        &self,
        viewer_user_id: i64,
        list_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .can_access_list
            .borrow()
            .get(&(viewer_user_id, list_id))
            .unwrap_or(&false))
    }

    fn invite_member(&self, _family_id: i64, _member_user_id: i64) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn remove_family_member(
        &self,
        _family_id: i64,
        _member_user_id: i64,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn is_user_in_family(&self, _user_id: i64) -> Result<bool, WishlistError> {
        Ok(false)
    }

    fn delete_family(&self, _family_id: i64, _owner_id: i64) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn leave_family(&self, _member_user_id: i64) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn rename_family(
        &self,
        _family_id: i64,
        _owner_id: i64,
        _new_name: &str,
    ) -> Result<Option<wishlist_core::families::FamilySummary>, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn get_pending_invitation(
        &self,
        _invited_user_id: i64,
    ) -> Result<Option<wishlist_core::families::PendingInvitation>, WishlistError> {
        Ok(None)
    }

    fn accept_invitation(
        &self,
        _family_id: i64,
        _invited_user_id: i64,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn decline_invitation(
        &self,
        _family_id: i64,
        _invited_user_id: i64,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }
}

#[derive(Debug, Default)]
struct ListQueryRepo {
    public_lists: RefCell<HashMap<i64, Vec<wishlist_core::lists::PublicListSummary>>>,
    public_list_calls: Rc<Cell<usize>>,
}

impl ListQueryRepo {
    fn new() -> Self {
        Self {
            public_lists: RefCell::new(HashMap::new()),
            public_list_calls: Rc::new(Cell::new(0)),
        }
    }
}

impl ListRepository for ListQueryRepo {
    fn list_owner_lists(
        &self,
        _owner_id: i64,
    ) -> Result<Vec<wishlist_core::lists::ListSummary>, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn list_public_lists(
        &self,
        owner_id: i64,
    ) -> Result<Vec<wishlist_core::lists::PublicListSummary>, WishlistError> {
        self.public_list_calls
            .set(self.public_list_calls.get().saturating_add(1));
        Ok(self
            .public_lists
            .borrow()
            .get(&owner_id)
            .cloned()
            .unwrap_or_default())
    }

    fn get_owner_list(
        &self,
        _owner_id: i64,
        _list_id: i64,
    ) -> Result<Option<wishlist_core::lists::ListSummary>, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn create_list(
        &self,
        _owner_id: i64,
        _name: &str,
        _is_private: bool,
    ) -> Result<wishlist_core::lists::ListSummary, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn rename_list(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _new_name: &str,
    ) -> Result<Option<wishlist_core::lists::ListSummary>, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn toggle_list_privacy(
        &self,
        _owner_id: i64,
        _list_id: i64,
    ) -> Result<Option<wishlist_core::lists::ListSummary>, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn delete_list(&self, _owner_id: i64, _list_id: i64) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }
}

#[derive(Debug, Default)]
struct WishQueryRepo {
    wishes_by_list: RefCell<HashMap<i64, Vec<wishlist_core::wishes::Wish>>>,
    wish_by_key: RefCell<HashMap<(i64, i64), Option<wishlist_core::wishes::Wish>>>,
    owner_by_key: RefCell<HashMap<(i64, i64), Option<i64>>>,
    reserve_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    unreserve_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    list_wishes_calls: Rc<Cell<usize>>,
    get_wish_calls: Rc<Cell<usize>>,
    reserve_calls: Rc<Cell<usize>>,
    unreserve_calls: Rc<Cell<usize>>,
}

impl WishQueryRepo {
    fn new() -> Self {
        Self {
            wishes_by_list: RefCell::new(HashMap::new()),
            wish_by_key: RefCell::new(HashMap::new()),
            owner_by_key: RefCell::new(HashMap::new()),
            reserve_results: RefCell::new(HashMap::new()),
            unreserve_results: RefCell::new(HashMap::new()),
            list_wishes_calls: Rc::new(Cell::new(0)),
            get_wish_calls: Rc::new(Cell::new(0)),
            reserve_calls: Rc::new(Cell::new(0)),
            unreserve_calls: Rc::new(Cell::new(0)),
        }
    }
}

impl WishRepository for WishQueryRepo {
    fn create_wish(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _title: &str,
    ) -> Result<i64, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn get_wish(
        &self,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Option<wishlist_core::wishes::Wish>, WishlistError> {
        self.get_wish_calls
            .set(self.get_wish_calls.get().saturating_add(1));
        Ok(self
            .wish_by_key
            .borrow()
            .get(&(list_id, wish_id))
            .cloned()
            .unwrap_or(None))
    }

    fn get_owner_wish(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _wish_id: i64,
    ) -> Result<Option<wishlist_core::wishes::Wish>, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn get_wish_owner(&self, list_id: i64, wish_id: i64) -> Result<Option<i64>, WishlistError> {
        Ok(self
            .owner_by_key
            .borrow()
            .get(&(list_id, wish_id))
            .cloned()
            .unwrap_or(None))
    }

    fn list_wishes(&self, list_id: i64) -> Result<Vec<wishlist_core::wishes::Wish>, WishlistError> {
        self.list_wishes_calls
            .set(self.list_wishes_calls.get().saturating_add(1));
        Ok(self
            .wishes_by_list
            .borrow()
            .get(&list_id)
            .cloned()
            .unwrap_or_default())
    }

    fn update_wish_title(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _wish_id: i64,
        _title: &str,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn update_wish_url(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _wish_id: i64,
        _url: Option<&str>,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn update_wish_priority(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _wish_id: i64,
        _priority: Option<i32>,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn update_wish_notes(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _wish_id: i64,
        _notes: Option<&str>,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn delete_wish(
        &self,
        _owner_id: i64,
        _list_id: i64,
        _wish_id: i64,
    ) -> Result<bool, WishlistError> {
        unreachable!("not used in WishlistService tests")
    }

    fn reserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError> {
        self.reserve_calls
            .set(self.reserve_calls.get().saturating_add(1));
        Ok(*self
            .reserve_results
            .borrow()
            .get(&(reserving_user_id, list_id, wish_id))
            .unwrap_or(&false))
    }

    fn unreserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError> {
        self.unreserve_calls
            .set(self.unreserve_calls.get().saturating_add(1));
        Ok(*self
            .unreserve_results
            .borrow()
            .get(&(reserving_user_id, list_id, wish_id))
            .unwrap_or(&false))
    }
}

#[test]
fn browse_member_lists_returns_access_denied_without_list_lookup() {
    let family_repo = FamilyAccessRepo::default();
    let list_repo = ListQueryRepo::new();
    let list_calls = Rc::clone(&list_repo.public_list_calls);
    let wish_repo = WishQueryRepo::new();
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    assert_eq!(
        service.browse_member_lists(7, 11),
        Err(WishlistError::AccessDenied)
    );
    assert_eq!(list_calls.get(), 0);
}

#[test]
fn browse_member_lists_returns_public_lists_on_success() {
    let family_repo = FamilyAccessRepo::default();
    family_repo.can_browse.borrow_mut().insert((7, 11), true);
    let list_repo = ListQueryRepo::new();
    list_repo
        .public_lists
        .borrow_mut()
        .insert(11, vec![public_list(5, "Birthday")]);
    let wish_repo = WishQueryRepo::new();
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    let result = service.browse_member_lists(7, 11).unwrap();

    assert_eq!(result, vec![public_list(5, "Birthday")]);
}

#[test]
fn browse_member_wishes_denies_access_without_wish_lookup() {
    let family_repo = FamilyAccessRepo::default();
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    let list_wishes_calls = Rc::clone(&wish_repo.list_wishes_calls);
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    assert_eq!(
        service.browse_member_wishes(7, 5),
        Err(WishlistError::AccessDenied)
    );
    assert_eq!(list_wishes_calls.get(), 0);
}

#[test]
fn browse_member_wishes_delegates_when_access_is_allowed() {
    let family_repo = FamilyAccessRepo::default();
    family_repo
        .can_access_list
        .borrow_mut()
        .insert((7, 5), true);
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    wish_repo
        .wishes_by_list
        .borrow_mut()
        .insert(5, vec![wish(9, "Steam Deck", None)]);
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    let result = service.browse_member_wishes(7, 5).unwrap();

    assert_eq!(result, vec![wish(9, "Steam Deck", None)]);
}

#[test]
fn view_member_wish_denies_access_without_wish_lookup() {
    let family_repo = FamilyAccessRepo::default();
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    let get_wish_calls = Rc::clone(&wish_repo.get_wish_calls);
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    assert_eq!(
        service.view_member_wish(7, 5, 9),
        Err(WishlistError::AccessDenied)
    );
    assert_eq!(get_wish_calls.get(), 0);
}

#[test]
fn view_member_wish_delegates_when_access_is_allowed() {
    let family_repo = FamilyAccessRepo::default();
    family_repo
        .can_access_list
        .borrow_mut()
        .insert((7, 5), true);
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    wish_repo
        .wish_by_key
        .borrow_mut()
        .insert((5, 9), Some(wish(9, "Steam Deck", None)));
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    let result = service.view_member_wish(7, 5, 9).unwrap();

    assert_eq!(result, Some(wish(9, "Steam Deck", None)));
}

#[test]
fn reserve_family_wish_denies_access_before_wish_mutation() {
    let family_repo = FamilyAccessRepo::default();
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    let reserve_calls = Rc::clone(&wish_repo.reserve_calls);
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    assert_eq!(
        service.reserve_family_wish(7, 5, 9),
        Err(WishlistError::AccessDenied)
    );
    assert_eq!(reserve_calls.get(), 0);
}

#[test]
fn reserve_family_wish_delegates_when_access_is_allowed() {
    let family_repo = FamilyAccessRepo::default();
    family_repo
        .can_access_list
        .borrow_mut()
        .insert((7, 5), true);
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    wish_repo.owner_by_key.borrow_mut().insert((5, 9), Some(11));
    wish_repo
        .reserve_results
        .borrow_mut()
        .insert((7, 5, 9), true);
    wish_repo
        .wish_by_key
        .borrow_mut()
        .insert((5, 9), Some(wish(9, "Steam Deck", Some(7))));
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    let result = service.reserve_family_wish(7, 5, 9).unwrap();

    assert!(matches!(result, ReserveWishResult::Reserved { .. }));
}

#[test]
fn unreserve_family_wish_denies_access_before_wish_mutation() {
    let family_repo = FamilyAccessRepo::default();
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    let unreserve_calls = Rc::clone(&wish_repo.unreserve_calls);
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    assert_eq!(
        service.unreserve_family_wish(7, 5, 9),
        Err(WishlistError::AccessDenied)
    );
    assert_eq!(unreserve_calls.get(), 0);
}

#[test]
fn unreserve_family_wish_delegates_when_access_is_allowed() {
    let family_repo = FamilyAccessRepo::default();
    family_repo
        .can_access_list
        .borrow_mut()
        .insert((7, 5), true);
    let list_repo = ListQueryRepo::new();
    let wish_repo = WishQueryRepo::new();
    wish_repo
        .unreserve_results
        .borrow_mut()
        .insert((7, 5, 9), true);
    wish_repo
        .wish_by_key
        .borrow_mut()
        .insert((5, 9), Some(wish(9, "Steam Deck", None)));
    let service = WishlistService::new(
        FamilyService::new(Box::new(family_repo)),
        ListService::new(Box::new(list_repo)),
        WishService::new(Box::new(wish_repo)),
    );

    let result = service.unreserve_family_wish(7, 5, 9).unwrap();

    assert!(matches!(result, UnreserveWishResult::Unreserved(_)));
}
