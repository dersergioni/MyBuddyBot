mod support;

use std::cell::{Cell, RefCell};
use std::collections::HashMap;

use support::{assert_validation_error, list};
use wishlist_core::WishlistError;
use wishlist_core::lists::{ListRepository, ListService};

#[derive(Debug)]
struct ListRepo {
    next_list_id: Cell<i64>,
    create_calls: RefCell<Vec<(i64, String, bool)>>,
    rename_calls: RefCell<Vec<(i64, i64, String)>>,
    create_results: RefCell<HashMap<(i64, bool), wishlist_core::lists::ListSummary>>,
    rename_results: RefCell<HashMap<(i64, i64), Option<wishlist_core::lists::ListSummary>>>,
    toggle_results: RefCell<HashMap<(i64, i64), Option<wishlist_core::lists::ListSummary>>>,
    delete_results: RefCell<HashMap<(i64, i64), bool>>,
}

impl Default for ListRepo {
    fn default() -> Self {
        Self {
            next_list_id: Cell::new(1),
            create_calls: RefCell::new(Vec::new()),
            rename_calls: RefCell::new(Vec::new()),
            create_results: RefCell::new(HashMap::new()),
            rename_results: RefCell::new(HashMap::new()),
            toggle_results: RefCell::new(HashMap::new()),
            delete_results: RefCell::new(HashMap::new()),
        }
    }
}

impl ListRepository for ListRepo {
    fn list_owner_lists(
        &self,
        _owner_id: i64,
    ) -> Result<Vec<wishlist_core::lists::ListSummary>, WishlistError> {
        Ok(Vec::new())
    }

    fn list_public_lists(
        &self,
        _owner_id: i64,
    ) -> Result<Vec<wishlist_core::lists::PublicListSummary>, WishlistError> {
        Ok(Vec::new())
    }

    fn get_owner_list(
        &self,
        _owner_id: i64,
        _list_id: i64,
    ) -> Result<Option<wishlist_core::lists::ListSummary>, WishlistError> {
        Ok(None)
    }

    fn create_list(
        &self,
        owner_id: i64,
        name: &str,
        is_private: bool,
    ) -> Result<wishlist_core::lists::ListSummary, WishlistError> {
        self.create_calls
            .borrow_mut()
            .push((owner_id, name.to_string(), is_private));
        Ok(self
            .create_results
            .borrow()
            .get(&(owner_id, is_private))
            .cloned()
            .unwrap_or_else(|| list(self.next_list_id.get(), name, is_private)))
    }

    fn rename_list(
        &self,
        owner_id: i64,
        list_id: i64,
        new_name: &str,
    ) -> Result<Option<wishlist_core::lists::ListSummary>, WishlistError> {
        self.rename_calls
            .borrow_mut()
            .push((owner_id, list_id, new_name.to_string()));
        Ok(self
            .rename_results
            .borrow()
            .get(&(owner_id, list_id))
            .cloned()
            .unwrap_or(None))
    }

    fn toggle_list_privacy(
        &self,
        owner_id: i64,
        list_id: i64,
    ) -> Result<Option<wishlist_core::lists::ListSummary>, WishlistError> {
        Ok(self
            .toggle_results
            .borrow()
            .get(&(owner_id, list_id))
            .cloned()
            .unwrap_or(None))
    }

    fn delete_list(&self, owner_id: i64, list_id: i64) -> Result<bool, WishlistError> {
        Ok(*self
            .delete_results
            .borrow()
            .get(&(owner_id, list_id))
            .unwrap_or(&false))
    }
}

#[test]
fn create_list_trims_name_and_returns_created_value() {
    let repo = ListRepo::default();
    repo.next_list_id.set(15);
    let service = ListService::new(Box::new(repo));

    let result = service.create_list(7, "  Birthday  ", true).unwrap();

    assert_eq!(result, list(15, "Birthday", true));
}

#[test]
fn create_list_rejects_empty_name() {
    let repo = ListRepo::default();
    let service = ListService::new(Box::new(repo));

    assert_validation_error(
        service.create_list(7, "   ", false).map(|_| ()),
        "List name cannot be empty",
    );
}

#[test]
fn rename_list_trims_name_and_returns_updated_value() {
    let repo = ListRepo::default();
    repo.rename_results
        .borrow_mut()
        .insert((7, 15), Some(list(15, "Updated", false)));
    let service = ListService::new(Box::new(repo));

    let result = service.rename_list(7, 15, "  Updated  ").unwrap();

    assert_eq!(result, list(15, "Updated", false));
}

#[test]
fn rename_list_rejects_empty_name() {
    let repo = ListRepo::default();
    let service = ListService::new(Box::new(repo));

    assert_validation_error(
        service.rename_list(7, 15, "   ").map(|_| ()),
        "List name cannot be empty",
    );
}

#[test]
fn toggle_list_privacy_returns_updated_list() {
    let repo = ListRepo::default();
    repo.toggle_results
        .borrow_mut()
        .insert((7, 15), Some(list(15, "Birthday", true)));
    let service = ListService::new(Box::new(repo));

    let result = service.toggle_list_privacy(7, 15).unwrap();

    assert_eq!(result, list(15, "Birthday", true));
}

#[test]
fn toggle_list_privacy_returns_not_found_when_list_is_missing() {
    let repo = ListRepo::default();
    let service = ListService::new(Box::new(repo));

    assert_eq!(
        service.toggle_list_privacy(7, 15),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn rename_list_returns_not_found_when_list_is_missing() {
    let repo = ListRepo::default();
    let service = ListService::new(Box::new(repo));

    assert_eq!(
        service.rename_list(7, 15, "New Name"),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn delete_list_returns_deleted_value() {
    let repo = ListRepo::default();
    repo.delete_results.borrow_mut().insert((7, 15), true);
    let service = ListService::new(Box::new(repo));

    service.delete_list(7, 15).unwrap();
}

#[test]
fn delete_list_returns_not_found_when_repository_reports_missing() {
    let repo = ListRepo::default();
    let service = ListService::new(Box::new(repo));

    assert_eq!(service.delete_list(7, 15), Err(WishlistError::NotFound));
}
