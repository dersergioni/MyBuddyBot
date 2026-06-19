mod support;

use std::collections::HashSet;

use support::{TestDb, create_family};
use wishlist_core::families::FamilyRepository;

#[test]
fn initializes_schema_indexes_and_foreign_keys() {
    let db = TestDb::new();
    let _store = db.repo();
    let conn = db.raw_connection();

    let foreign_keys: i64 = conn
        .query_row("PRAGMA foreign_keys;", [], |row| row.get(0))
        .unwrap();
    assert_eq!(foreign_keys, 1);

    let mut stmt = conn
        .prepare("SELECT name FROM sqlite_master WHERE type IN ('table', 'index')")
        .unwrap();
    let names = stmt
        .query_map([], |row| row.get::<_, String>(0))
        .unwrap()
        .collect::<Result<HashSet<_>, _>>()
        .unwrap();

    for expected in [
        "lists",
        "wishes",
        "families",
        "family_members",
        "idx_families_owner",
        "idx_lists_owner",
        "idx_wishes_list",
        "idx_family_members_user",
    ] {
        assert!(names.contains(expected), "missing schema object {expected}");
    }
}

#[test]
fn reopening_existing_database_is_idempotent() {
    let db = TestDb::new();
    {
        let store = db.repo();
        let family_repo = store.family_repo();
        let family_id = create_family(&store, 7, "Crew");
        assert_eq!(
            FamilyRepository::get_family_for_user(&family_repo, 7)
                .unwrap()
                .unwrap()
                .id,
            family_id
        );
    }

    let store = db.repo();
    let family_repo = store.family_repo();
    let family = FamilyRepository::get_family_for_user(&family_repo, 7)
        .unwrap()
        .expect("family should persist after reopening");
    assert_eq!(family.name, "Crew");
}
