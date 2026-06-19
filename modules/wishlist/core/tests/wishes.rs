mod support;

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::rc::Rc;

use support::{assert_validation_error, wish};
use wishlist_core::WishlistError;
use wishlist_core::wishes::{ReserveWishResult, UnreserveWishResult, WishRepository, WishService};

type CallLog3 = Rc<RefCell<Vec<(i64, i64, String)>>>;
type CallLog4 = Rc<RefCell<Vec<(i64, i64, i64, String)>>>;

#[derive(Debug)]
struct WishRepo {
    next_wish_id: Cell<i64>,
    create_calls: CallLog3,
    update_title_calls: CallLog4,
    create_results: RefCell<HashMap<(i64, i64), Option<i64>>>, // None = fail, Some(id) = success
    owner_wish_by_key: RefCell<HashMap<(i64, i64, i64), Option<wishlist_core::wishes::Wish>>>,
    wish_by_key: RefCell<HashMap<(i64, i64), Option<wishlist_core::wishes::Wish>>>,
    owner_by_key: RefCell<HashMap<(i64, i64), Option<i64>>>,
    update_title_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    update_url_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    update_priority_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    update_notes_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    delete_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    reserve_results: RefCell<HashMap<(i64, i64, i64), bool>>,
    unreserve_results: RefCell<HashMap<(i64, i64, i64), bool>>,
}

impl Default for WishRepo {
    fn default() -> Self {
        Self {
            next_wish_id: Cell::new(1),
            create_calls: Rc::new(RefCell::new(Vec::new())),
            update_title_calls: Rc::new(RefCell::new(Vec::new())),
            create_results: RefCell::new(HashMap::new()),
            owner_wish_by_key: RefCell::new(HashMap::new()),
            wish_by_key: RefCell::new(HashMap::new()),
            owner_by_key: RefCell::new(HashMap::new()),
            update_title_results: RefCell::new(HashMap::new()),
            update_url_results: RefCell::new(HashMap::new()),
            update_priority_results: RefCell::new(HashMap::new()),
            update_notes_results: RefCell::new(HashMap::new()),
            delete_results: RefCell::new(HashMap::new()),
            reserve_results: RefCell::new(HashMap::new()),
            unreserve_results: RefCell::new(HashMap::new()),
        }
    }
}

impl WishRepository for WishRepo {
    fn create_wish(&self, owner_id: i64, list_id: i64, title: &str) -> Result<i64, WishlistError> {
        self.create_calls
            .borrow_mut()
            .push((owner_id, list_id, title.to_string()));
        match self
            .create_results
            .borrow()
            .get(&(owner_id, list_id))
            .cloned()
        {
            Some(None) => Err(WishlistError::NotFound),
            Some(Some(id)) => Ok(id),
            None => Ok(self.next_wish_id.get()),
        }
    }

    fn get_wish(
        &self,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Option<wishlist_core::wishes::Wish>, WishlistError> {
        Ok(self
            .wish_by_key
            .borrow()
            .get(&(list_id, wish_id))
            .cloned()
            .unwrap_or(None))
    }

    fn get_owner_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<Option<wishlist_core::wishes::Wish>, WishlistError> {
        Ok(self
            .owner_wish_by_key
            .borrow()
            .get(&(owner_id, list_id, wish_id))
            .cloned()
            .unwrap_or(None))
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
        Ok(self
            .wish_by_key
            .borrow()
            .iter()
            .filter_map(|((stored_list_id, _), value)| {
                if *stored_list_id == list_id {
                    value.clone()
                } else {
                    None
                }
            })
            .collect())
    }

    fn update_wish_title(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        title: &str,
    ) -> Result<bool, WishlistError> {
        self.update_title_calls
            .borrow_mut()
            .push((owner_id, list_id, wish_id, title.to_string()));
        Ok(*self
            .update_title_results
            .borrow()
            .get(&(owner_id, list_id, wish_id))
            .unwrap_or(&false))
    }

    fn update_wish_url(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        _url: Option<&str>,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .update_url_results
            .borrow()
            .get(&(owner_id, list_id, wish_id))
            .unwrap_or(&false))
    }

    fn update_wish_priority(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        _priority: Option<i32>,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .update_priority_results
            .borrow()
            .get(&(owner_id, list_id, wish_id))
            .unwrap_or(&false))
    }

    fn update_wish_notes(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
        _notes: Option<&str>,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .update_notes_results
            .borrow()
            .get(&(owner_id, list_id, wish_id))
            .unwrap_or(&false))
    }

    fn delete_wish(
        &self,
        owner_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .delete_results
            .borrow()
            .get(&(owner_id, list_id, wish_id))
            .unwrap_or(&false))
    }

    fn reserve_wish(
        &self,
        reserving_user_id: i64,
        list_id: i64,
        wish_id: i64,
    ) -> Result<bool, WishlistError> {
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
        Ok(*self
            .unreserve_results
            .borrow()
            .get(&(reserving_user_id, list_id, wish_id))
            .unwrap_or(&false))
    }
}

#[test]
fn create_wish_trims_title_and_returns_created_value() {
    let repo = WishRepo::default();
    let create_calls = Rc::clone(&repo.create_calls);
    repo.next_wish_id.set(33);
    let service = WishService::new(Box::new(repo));

    let result = service.create_wish(7, 11, "  Steam Deck  ").unwrap();

    assert_eq!(result, 33);
    assert_eq!(
        create_calls.borrow().as_slice(),
        &[(7, 11, "Steam Deck".to_string())]
    );
}

#[test]
fn create_wish_rejects_empty_title() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_validation_error(
        service.create_wish(7, 11, "   ").map(|_| ()),
        "Wish title cannot be empty",
    );
}

#[test]
fn create_wish_returns_not_found_when_list_does_not_belong_to_owner() {
    let repo = WishRepo::default();
    repo.create_results.borrow_mut().insert((7, 11), None);
    let service = WishService::new(Box::new(repo));

    assert_eq!(
        service.create_wish(7, 11, "Steam Deck"),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn update_title_trims_value_and_returns_updated_wish() {
    let repo = WishRepo::default();
    let update_title_calls = Rc::clone(&repo.update_title_calls);
    repo.update_title_results
        .borrow_mut()
        .insert((7, 11, 33), true);
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(wish(33, "Steam Deck", None)));
    let service = WishService::new(Box::new(repo));

    let result = service.update_title(7, 11, 33, "  Steam Deck  ").unwrap();

    assert_eq!(result, wish(33, "Steam Deck", None));
    assert_eq!(
        update_title_calls.borrow().as_slice(),
        &[(7, 11, 33, "Steam Deck".to_string())]
    );
}

#[test]
fn update_title_rejects_empty_value() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_validation_error(
        service.update_title(7, 11, 33, "   ").map(|_| ()),
        "Wish title cannot be empty",
    );
}

#[test]
fn update_title_returns_not_found_when_wish_is_missing() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_eq!(
        service.update_title(7, 11, 33, "New Title"),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn update_url_returns_updated_wish() {
    let repo = WishRepo::default();
    let mut updated = wish(33, "Steam Deck", None);
    updated.url = Some("https://store.steampowered.com".to_string());
    repo.update_url_results
        .borrow_mut()
        .insert((7, 11, 33), true);
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(updated.clone()));
    let service = WishService::new(Box::new(repo));

    let result = service
        .update_url(7, 11, 33, Some("https://store.steampowered.com"))
        .unwrap();

    assert_eq!(result, updated);
}

#[test]
fn update_url_returns_not_found_when_wish_is_missing() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_eq!(
        service.update_url(7, 11, 33, Some("https://example.com")),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn update_priority_rejects_values_out_of_range() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_validation_error(
        service.update_priority(7, 11, 33, Some(0)).map(|_| ()),
        "Wish priority must be between 1 and 3",
    );
    assert_validation_error(
        service.update_priority(7, 11, 33, Some(4)).map(|_| ()),
        "Wish priority must be between 1 and 3",
    );
}

#[test]
fn update_priority_accepts_none_to_clear_priority() {
    let repo = WishRepo::default();
    repo.update_priority_results
        .borrow_mut()
        .insert((7, 11, 33), true);
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(wish(33, "Steam Deck", None)));
    let service = WishService::new(Box::new(repo));

    let result = service.update_priority(7, 11, 33, None).unwrap();

    assert_eq!(result, wish(33, "Steam Deck", None));
}

#[test]
fn update_priority_returns_updated_wish() {
    let repo = WishRepo::default();
    let mut updated = wish(33, "Steam Deck", None);
    updated.priority = Some(2);
    repo.update_priority_results
        .borrow_mut()
        .insert((7, 11, 33), true);
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(updated.clone()));
    let service = WishService::new(Box::new(repo));

    let result = service.update_priority(7, 11, 33, Some(2)).unwrap();

    assert_eq!(result, updated);
}

#[test]
fn update_priority_returns_not_found_when_wish_is_missing() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_eq!(
        service.update_priority(7, 11, 33, Some(2)),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn update_notes_returns_updated_wish() {
    let repo = WishRepo::default();
    let mut updated = wish(33, "Steam Deck", None);
    updated.notes = Some("Black color".to_string());
    repo.update_notes_results
        .borrow_mut()
        .insert((7, 11, 33), true);
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(updated.clone()));
    let service = WishService::new(Box::new(repo));

    let result = service
        .update_notes(7, 11, 33, Some("Black color"))
        .unwrap();

    assert_eq!(result, updated);
}

#[test]
fn update_notes_returns_not_found_when_wish_is_missing() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_eq!(
        service.update_notes(7, 11, 33, Some("Black color")),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn delete_wish_returns_not_found_when_repository_reports_missing() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_eq!(service.delete_wish(7, 11, 33), Err(WishlistError::NotFound));
}

#[test]
fn delete_wish_succeeds_when_repository_confirms_deletion() {
    let repo = WishRepo::default();
    repo.delete_results.borrow_mut().insert((7, 11, 33), true);
    let service = WishService::new(Box::new(repo));

    assert_eq!(service.delete_wish(7, 11, 33), Ok(()));
}

#[test]
fn toggle_owner_reservation_reserves_unreserved_wish() {
    let repo = WishRepo::default();
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(wish(33, "Steam Deck", None)));
    repo.reserve_results.borrow_mut().insert((7, 11, 33), true);
    let service = WishService::new(Box::new(repo));

    let result = service.toggle_owner_reservation(7, 11, 33).unwrap();

    assert_eq!(result, wish(33, "Steam Deck", Some(7)));
}

#[test]
fn toggle_owner_reservation_unreserves_when_owner_reserved_it() {
    let repo = WishRepo::default();
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(wish(33, "Steam Deck", Some(7))));
    repo.unreserve_results
        .borrow_mut()
        .insert((7, 11, 33), true);
    let service = WishService::new(Box::new(repo));

    let result = service.toggle_owner_reservation(7, 11, 33).unwrap();

    assert_eq!(result, wish(33, "Steam Deck", None));
}

#[test]
fn toggle_owner_reservation_keeps_existing_reservation_by_other_user() {
    let repo = WishRepo::default();
    repo.owner_wish_by_key
        .borrow_mut()
        .insert((7, 11, 33), Some(wish(33, "Steam Deck", Some(99))));
    let service = WishService::new(Box::new(repo));

    let result = service.toggle_owner_reservation(7, 11, 33).unwrap();

    assert_eq!(result, wish(33, "Steam Deck", Some(99)));
}

#[test]
fn reserve_wish_returns_not_found_when_owner_cannot_be_resolved() {
    let repo = WishRepo::default();
    let service = WishService::new(Box::new(repo));

    assert_eq!(
        service.reserve_wish(7, 11, 33),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn reserve_wish_returns_already_reserved_when_wish_is_taken() {
    let repo = WishRepo::default();
    repo.owner_by_key.borrow_mut().insert((11, 33), Some(5));
    repo.wish_by_key
        .borrow_mut()
        .insert((11, 33), Some(wish(33, "Steam Deck", Some(99))));
    let service = WishService::new(Box::new(repo));

    let result = service.reserve_wish(7, 11, 33).unwrap();

    assert_eq!(result, ReserveWishResult::AlreadyReserved);
}

#[test]
fn reserve_wish_emits_event_for_non_owner() {
    let repo = WishRepo::default();
    repo.owner_by_key.borrow_mut().insert((11, 33), Some(5));
    repo.reserve_results.borrow_mut().insert((7, 11, 33), true);
    repo.wish_by_key
        .borrow_mut()
        .insert((11, 33), Some(wish(33, "Steam Deck", Some(7))));
    let service = WishService::new(Box::new(repo));

    let result = service.reserve_wish(7, 11, 33).unwrap();

    assert_eq!(
        result,
        ReserveWishResult::Reserved {
            wish: wish(33, "Steam Deck", Some(7)),
            notify_owner_id: Some(5),
        }
    );
}

#[test]
fn reserve_wish_does_not_emit_event_for_owner() {
    let repo = WishRepo::default();
    repo.owner_by_key.borrow_mut().insert((11, 33), Some(7));
    repo.reserve_results.borrow_mut().insert((7, 11, 33), true);
    repo.wish_by_key
        .borrow_mut()
        .insert((11, 33), Some(wish(33, "Steam Deck", Some(7))));
    let service = WishService::new(Box::new(repo));

    let result = service.reserve_wish(7, 11, 33).unwrap();

    assert_eq!(
        result,
        ReserveWishResult::Reserved {
            wish: wish(33, "Steam Deck", Some(7)),
            notify_owner_id: None
        }
    );
}

#[test]
fn unreserve_wish_returns_unreserved_when_repository_updates_wish() {
    let repo = WishRepo::default();
    repo.unreserve_results
        .borrow_mut()
        .insert((7, 11, 33), true);
    repo.wish_by_key
        .borrow_mut()
        .insert((11, 33), Some(wish(33, "Steam Deck", None)));
    let service = WishService::new(Box::new(repo));

    let result = service.unreserve_wish(7, 11, 33).unwrap();

    assert_eq!(
        result,
        UnreserveWishResult::Unreserved(wish(33, "Steam Deck", None))
    );
}

#[test]
fn unreserve_wish_returns_not_reserved_by_you_when_user_did_not_reserve_it() {
    let repo = WishRepo::default();
    repo.wish_by_key
        .borrow_mut()
        .insert((11, 33), Some(wish(33, "Steam Deck", Some(99))));
    let service = WishService::new(Box::new(repo));

    let result = service.unreserve_wish(7, 11, 33).unwrap();

    assert_eq!(result, UnreserveWishResult::NotReservedByYou);
}
