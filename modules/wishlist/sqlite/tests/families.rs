mod support;

use support::{
    TestDb, accept_invitation, assert_storage_error, create_family, create_list, invite_member,
};
use wishlist_core::families::{FamilyRepository, FamilySummary, PendingInvitation};

#[test]
fn create_family_creates_owner_membership() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();

    let family_id = create_family(&store, 7, "Crew");

    let family = FamilyRepository::get_family_for_user(&family_repo, 7)
        .unwrap()
        .expect("owner should belong to created family");
    assert_eq!(family.id, family_id);
    assert_eq!(family.owner_id, 7);
    assert_eq!(family.name, "Crew");
    assert_eq!(
        FamilyRepository::get_family_members(&family_repo, family_id).unwrap(),
        vec![7]
    );
}

#[test]
fn pending_invitation_is_not_treated_as_accepted_membership_and_is_idempotent() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    let family_id = create_family(&store, 7, "Crew");

    assert!(FamilyRepository::invite_member(&family_repo, family_id, 11).unwrap());
    assert!(FamilyRepository::invite_member(&family_repo, family_id, 11).unwrap());

    assert_eq!(
        FamilyRepository::get_family_for_user(&family_repo, 11).unwrap(),
        None
    );
    assert!(!FamilyRepository::is_user_in_family(&family_repo, 11).unwrap());
    assert_eq!(
        FamilyRepository::get_pending_invitation(&family_repo, 11).unwrap(),
        Some(PendingInvitation {
            family_id,
            owner_id: 7,
            family_name: "Crew".to_string(),
        })
    );
}

#[test]
fn accept_and_decline_invitation_follow_the_expected_lifecycle() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    let family_id = create_family(&store, 7, "Crew");
    invite_member(&store, family_id, 11);
    invite_member(&store, family_id, 12);

    accept_invitation(&store, 11, family_id);
    assert!(FamilyRepository::is_user_in_family(&family_repo, 11).unwrap());
    assert_eq!(
        FamilyRepository::get_family_for_user(&family_repo, 11)
            .unwrap()
            .unwrap()
            .id,
        family_id
    );

    assert!(FamilyRepository::decline_invitation(&family_repo, family_id, 12).unwrap());
    assert_eq!(
        FamilyRepository::get_pending_invitation(&family_repo, 12).unwrap(),
        None
    );
    assert!(!FamilyRepository::is_user_in_family(&family_repo, 12).unwrap());
}

#[test]
fn browse_and_public_list_access_require_same_accepted_family() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    let family_id = create_family(&store, 7, "Crew");
    invite_member(&store, family_id, 11);
    accept_invitation(&store, 11, family_id);

    let outsider_family_id = create_family(&store, 21, "Other");
    invite_member(&store, outsider_family_id, 22);
    accept_invitation(&store, 22, outsider_family_id);

    let public_list = create_list(&store, 7, "Birthday", false);
    let private_list = create_list(&store, 7, "Private", true);

    assert!(FamilyRepository::can_browse_family_member(&family_repo, 11, 7).unwrap());
    assert!(!FamilyRepository::can_browse_family_member(&family_repo, 11, 22).unwrap());
    assert!(
        FamilyRepository::can_access_public_family_list(&family_repo, 11, public_list.id).unwrap()
    );
    assert!(
        !FamilyRepository::can_access_public_family_list(&family_repo, 11, private_list.id)
            .unwrap()
    );
    assert!(
        !FamilyRepository::can_access_public_family_list(&family_repo, 22, public_list.id).unwrap()
    );
}

#[test]
fn member_can_leave_family_but_owner_cannot() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    let family_id = create_family(&store, 7, "Crew");
    invite_member(&store, family_id, 11);
    accept_invitation(&store, 11, family_id);

    assert!(FamilyRepository::leave_family(&family_repo, 11).unwrap());
    assert!(!FamilyRepository::is_user_in_family(&family_repo, 11).unwrap());
    assert!(!FamilyRepository::leave_family(&family_repo, 7).unwrap());
}

#[test]
fn delete_family_cascades_all_memberships() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    let family_id = create_family(&store, 7, "Crew");
    invite_member(&store, family_id, 12);
    accept_invitation(&store, 12, family_id);

    assert!(FamilyRepository::delete_family(&family_repo, family_id, 7).unwrap());
    assert_eq!(
        FamilyRepository::get_family_for_user(&family_repo, 7).unwrap(),
        None
    );
    assert_eq!(
        FamilyRepository::get_family_for_user(&family_repo, 12).unwrap(),
        None
    );
}

#[test]
fn rename_family_returns_updated_summary_when_owner_matches() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    let family_id = create_family(&store, 7, "Crew");

    let result = FamilyRepository::rename_family(&family_repo, family_id, 7, "Crew 2.0").unwrap();

    assert_eq!(
        result,
        Some(FamilySummary {
            id: family_id,
            owner_id: 7,
            name: "Crew 2.0".to_string()
        })
    );
}

#[test]
fn rename_family_returns_none_when_user_is_not_owner() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    let family_id = create_family(&store, 7, "Crew");

    let result = FamilyRepository::rename_family(&family_repo, family_id, 99, "Hijacked").unwrap();

    assert_eq!(result, None);
}

#[test]
fn duplicate_family_owner_is_rejected_by_database_constraint() {
    let db = TestDb::new();
    let store = db.repo();
    let family_repo = store.family_repo();
    create_family(&store, 7, "Crew");

    assert_storage_error(
        FamilyRepository::create_family(&family_repo, 7, "Another").map(|_| ()),
        "UNIQUE constraint failed",
    );
}
