mod support;

use support::{TestDb, assert_storage_error, create_list, create_wish};
use wishlist_core::wishes::{Wish, WishRepository};

#[test]
fn create_wish_requires_owner_list_and_exposes_owner_scope_queries() {
    let db = TestDb::new();
    let store = db.repo();
    let wish_repo = store.wish_repo();
    let list = create_list(&store, 7, "Birthday", false);

    let wish_id = create_wish(&store, 7, list.id, "Steam Deck");
    assert_eq!(
        WishRepository::get_wish(&wish_repo, list.id, wish_id).unwrap(),
        Some(Wish {
            id: wish_id,
            title: "Steam Deck".to_string(),
            url: None,
            priority: None,
            notes: None,
            reserved_by: None,
        })
    );
    assert!(
        WishRepository::get_owner_wish(&wish_repo, 7, list.id, wish_id)
            .unwrap()
            .is_some()
    );
    assert_eq!(
        WishRepository::get_owner_wish(&wish_repo, 8, list.id, wish_id).unwrap(),
        None
    );
    assert_eq!(
        WishRepository::get_wish_owner(&wish_repo, list.id, wish_id).unwrap(),
        Some(7)
    );
    assert_eq!(
        WishRepository::create_wish(&wish_repo, 8, list.id, "No access"),
        Err(wishlist_core::WishlistError::NotFound)
    );
}

#[test]
fn update_operations_round_trip_all_mutable_wish_fields() {
    let db = TestDb::new();
    let store = db.repo();
    let wish_repo = store.wish_repo();
    let list = create_list(&store, 7, "Birthday", false);
    let wish_id = create_wish(&store, 7, list.id, "Steam Deck");

    assert!(WishRepository::update_wish_title(&wish_repo, 7, list.id, wish_id, "OLED").unwrap());
    assert!(
        WishRepository::update_wish_url(
            &wish_repo,
            7,
            list.id,
            wish_id,
            Some("https://example.com")
        )
        .unwrap()
    );
    assert!(
        WishRepository::update_wish_priority(&wish_repo, 7, list.id, wish_id, Some(2)).unwrap()
    );
    assert!(
        WishRepository::update_wish_notes(&wish_repo, 7, list.id, wish_id, Some("Black")).unwrap()
    );

    let updated = WishRepository::get_wish(&wish_repo, list.id, wish_id)
        .unwrap()
        .expect("wish should exist");
    assert_eq!(updated.title, "OLED");
    assert_eq!(updated.url.as_deref(), Some("https://example.com"));
    assert_eq!(updated.priority, Some(2));
    assert_eq!(updated.notes.as_deref(), Some("Black"));
}

#[test]
fn update_and_delete_are_owner_scoped() {
    let db = TestDb::new();
    let store = db.repo();
    let wish_repo = store.wish_repo();
    let list = create_list(&store, 7, "Birthday", false);
    let wish_id = create_wish(&store, 7, list.id, "Steam Deck");

    assert!(!WishRepository::update_wish_title(&wish_repo, 8, list.id, wish_id, "Nope").unwrap());
    assert!(!WishRepository::delete_wish(&wish_repo, 8, list.id, wish_id).unwrap());
    assert!(WishRepository::delete_wish(&wish_repo, 7, list.id, wish_id).unwrap());
    assert_eq!(
        WishRepository::get_wish(&wish_repo, list.id, wish_id).unwrap(),
        None
    );
}

#[test]
fn reserve_and_unreserve_require_correct_current_state_and_reserving_user() {
    let db = TestDb::new();
    let store = db.repo();
    let wish_repo = store.wish_repo();
    let list = create_list(&store, 7, "Birthday", false);
    let wish_id = create_wish(&store, 7, list.id, "Steam Deck");

    assert!(WishRepository::reserve_wish(&wish_repo, 11, list.id, wish_id).unwrap());
    assert!(!WishRepository::reserve_wish(&wish_repo, 12, list.id, wish_id).unwrap());
    assert_eq!(
        WishRepository::get_wish(&wish_repo, list.id, wish_id)
            .unwrap()
            .unwrap()
            .reserved_by,
        Some(11)
    );

    assert!(!WishRepository::unreserve_wish(&wish_repo, 12, list.id, wish_id).unwrap());
    assert!(WishRepository::unreserve_wish(&wish_repo, 11, list.id, wish_id).unwrap());
    assert_eq!(
        WishRepository::get_wish(&wish_repo, list.id, wish_id)
            .unwrap()
            .unwrap()
            .reserved_by,
        None
    );
}

#[test]
fn out_of_range_priority_is_rejected_by_database_constraint() {
    let db = TestDb::new();
    let store = db.repo();
    let wish_repo = store.wish_repo();
    let list = create_list(&store, 7, "Birthday", false);
    let wish_id = create_wish(&store, 7, list.id, "Steam Deck");

    assert_storage_error(
        WishRepository::update_wish_priority(&wish_repo, 7, list.id, wish_id, Some(4)).map(|_| ()),
        "CHECK constraint failed",
    );
}

#[test]
fn empty_wish_title_is_rejected_by_database_constraint() {
    let db = TestDb::new();
    let store = db.repo();
    let wish_repo = store.wish_repo();
    let list = create_list(&store, 7, "Birthday", false);

    assert_storage_error(
        WishRepository::create_wish(&wish_repo, 7, list.id, "").map(|_| ()),
        "CHECK constraint failed",
    );
}
