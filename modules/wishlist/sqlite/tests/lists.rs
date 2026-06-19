mod support;

use support::{TestDb, assert_storage_error, create_list, create_wish};
use wishlist_core::lists::ListRepository;
use wishlist_core::wishes::WishRepository;

#[test]
fn create_and_query_owner_lists_respect_visibility_and_owner_scope() {
    let db = TestDb::new();
    let store = db.repo();
    let list_repo = store.list_repo();

    let public_list = create_list(&store, 7, "Birthday", false);
    let private_list = create_list(&store, 7, "Private", true);

    assert_eq!(
        ListRepository::list_owner_lists(&list_repo, 7).unwrap(),
        vec![public_list.clone(), private_list.clone()]
    );
    assert_eq!(
        ListRepository::list_public_lists(&list_repo, 7).unwrap(),
        vec![wishlist_core::lists::PublicListSummary {
            id: public_list.id,
            name: public_list.name.clone(),
        }]
    );
    assert_eq!(
        ListRepository::get_owner_list(&list_repo, 7, public_list.id).unwrap(),
        Some(public_list.clone())
    );
    assert_eq!(
        ListRepository::get_owner_list(&list_repo, 8, public_list.id).unwrap(),
        None
    );
}

#[test]
fn rename_toggle_and_delete_are_owner_scoped() {
    let db = TestDb::new();
    let store = db.repo();
    let list_repo = store.list_repo();
    let created = create_list(&store, 7, "Birthday", false);

    let renamed = ListRepository::rename_list(&list_repo, 7, created.id, "Updated")
        .unwrap()
        .expect("owner should rename list");
    assert_eq!(renamed.name, "Updated");
    assert_eq!(
        ListRepository::rename_list(&list_repo, 8, created.id, "Nope").unwrap(),
        None
    );

    let toggled = ListRepository::toggle_list_privacy(&list_repo, 7, created.id)
        .unwrap()
        .expect("owner should toggle list privacy");
    assert!(toggled.is_private);
    assert_eq!(
        ListRepository::toggle_list_privacy(&list_repo, 8, created.id).unwrap(),
        None
    );

    assert!(!ListRepository::delete_list(&list_repo, 8, created.id).unwrap());
    assert!(ListRepository::delete_list(&list_repo, 7, created.id).unwrap());
}

#[test]
fn deleting_list_cascades_its_wishes() {
    let db = TestDb::new();
    let store = db.repo();
    let list_repo = store.list_repo();
    let wish_repo = store.wish_repo();
    let list = create_list(&store, 7, "Birthday", false);
    let wish_id = create_wish(&store, 7, list.id, "Steam Deck");

    assert!(ListRepository::delete_list(&list_repo, 7, list.id).unwrap());
    assert_eq!(
        WishRepository::get_wish(&wish_repo, list.id, wish_id).unwrap(),
        None
    );
}

#[test]
fn empty_list_name_is_rejected_by_database_constraint() {
    let db = TestDb::new();
    let store = db.repo();
    let list_repo = store.list_repo();

    assert_storage_error(
        ListRepository::create_list(&list_repo, 7, "", false).map(|_| ()),
        "CHECK constraint failed",
    );
}
