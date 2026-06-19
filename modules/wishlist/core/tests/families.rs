mod support;

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::rc::Rc;

use support::{assert_validation_error, family, invitation};
use wishlist_core::WishlistError;
use wishlist_core::families::{
    AcceptInvitationResult, CreateFamilyResult, DeclineInvitationResult, DeleteFamilyResult,
    FamilyRepository, FamilyService, FamilySummary, InviteMemberResult, LeaveFamilyResult,
    RemoveMemberResult,
};

#[derive(Debug)]
struct FamilyRepo {
    next_family_id: Cell<i64>,
    create_calls: Rc<RefCell<Vec<(i64, String)>>>,
    rename_calls: Rc<RefCell<Vec<(i64, i64, String)>>>,
    family_for_user: RefCell<HashMap<i64, Option<wishlist_core::families::FamilySummary>>>,
    pending_invitation: RefCell<HashMap<i64, Option<wishlist_core::families::PendingInvitation>>>,
    is_in_family: RefCell<HashMap<i64, bool>>,
    invite_results: RefCell<HashMap<(i64, i64), bool>>,
    accept_results: RefCell<HashMap<(i64, i64), bool>>,
    decline_results: RefCell<HashMap<(i64, i64), bool>>,
    remove_results: RefCell<HashMap<(i64, i64), bool>>,
    leave_results: RefCell<HashMap<i64, bool>>,
    rename_results: RefCell<HashMap<(i64, i64), bool>>,
    delete_results: RefCell<HashMap<(i64, i64), bool>>,
}

impl Default for FamilyRepo {
    fn default() -> Self {
        Self {
            next_family_id: Cell::new(1),
            create_calls: Rc::new(RefCell::new(Vec::new())),
            rename_calls: Rc::new(RefCell::new(Vec::new())),
            family_for_user: RefCell::new(HashMap::new()),
            pending_invitation: RefCell::new(HashMap::new()),
            is_in_family: RefCell::new(HashMap::new()),
            invite_results: RefCell::new(HashMap::new()),
            accept_results: RefCell::new(HashMap::new()),
            decline_results: RefCell::new(HashMap::new()),
            remove_results: RefCell::new(HashMap::new()),
            leave_results: RefCell::new(HashMap::new()),
            rename_results: RefCell::new(HashMap::new()),
            delete_results: RefCell::new(HashMap::new()),
        }
    }
}

impl FamilyRepository for FamilyRepo {
    fn create_family(&self, owner_id: i64, name: &str) -> Result<i64, WishlistError> {
        self.create_calls
            .borrow_mut()
            .push((owner_id, name.to_string()));
        Ok(self.next_family_id.get())
    }

    fn get_family_for_user(
        &self,
        user_id: i64,
    ) -> Result<Option<wishlist_core::families::FamilySummary>, WishlistError> {
        Ok(self
            .family_for_user
            .borrow()
            .get(&user_id)
            .cloned()
            .unwrap_or(None))
    }

    fn get_family_members(&self, _family_id: i64) -> Result<Vec<i64>, WishlistError> {
        Ok(Vec::new())
    }

    fn can_browse_family_member(
        &self,
        _viewer_user_id: i64,
        _member_user_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(false)
    }

    fn can_access_public_family_list(
        &self,
        _viewer_user_id: i64,
        _list_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(false)
    }

    fn invite_member(&self, family_id: i64, member_user_id: i64) -> Result<bool, WishlistError> {
        Ok(*self
            .invite_results
            .borrow()
            .get(&(family_id, member_user_id))
            .unwrap_or(&false))
    }

    fn remove_family_member(
        &self,
        family_id: i64,
        member_user_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .remove_results
            .borrow()
            .get(&(family_id, member_user_id))
            .unwrap_or(&false))
    }

    fn is_user_in_family(&self, user_id: i64) -> Result<bool, WishlistError> {
        Ok(*self.is_in_family.borrow().get(&user_id).unwrap_or(&false))
    }

    fn delete_family(&self, family_id: i64, owner_id: i64) -> Result<bool, WishlistError> {
        Ok(*self
            .delete_results
            .borrow()
            .get(&(family_id, owner_id))
            .unwrap_or(&false))
    }

    fn leave_family(&self, member_user_id: i64) -> Result<bool, WishlistError> {
        Ok(*self
            .leave_results
            .borrow()
            .get(&member_user_id)
            .unwrap_or(&false))
    }

    fn rename_family(
        &self,
        family_id: i64,
        owner_id: i64,
        new_name: &str,
    ) -> Result<Option<FamilySummary>, WishlistError> {
        self.rename_calls
            .borrow_mut()
            .push((family_id, owner_id, new_name.to_string()));
        let found = *self
            .rename_results
            .borrow()
            .get(&(family_id, owner_id))
            .unwrap_or(&false);
        Ok(if found {
            Some(FamilySummary {
                id: family_id,
                owner_id,
                name: new_name.to_string(),
            })
        } else {
            None
        })
    }

    fn get_pending_invitation(
        &self,
        invited_user_id: i64,
    ) -> Result<Option<wishlist_core::families::PendingInvitation>, WishlistError> {
        Ok(self
            .pending_invitation
            .borrow()
            .get(&invited_user_id)
            .cloned()
            .unwrap_or(None))
    }

    fn accept_invitation(
        &self,
        family_id: i64,
        invited_user_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .accept_results
            .borrow()
            .get(&(family_id, invited_user_id))
            .unwrap_or(&false))
    }

    fn decline_invitation(
        &self,
        family_id: i64,
        invited_user_id: i64,
    ) -> Result<bool, WishlistError> {
        Ok(*self
            .decline_results
            .borrow()
            .get(&(family_id, invited_user_id))
            .unwrap_or(&false))
    }
}

#[test]
fn create_family_trims_name_and_returns_created_result() {
    let repo = FamilyRepo::default();
    let create_calls = Rc::clone(&repo.create_calls);
    repo.next_family_id.set(42);
    let service = FamilyService::new(Box::new(repo));

    let result = service.create_family(7, "  Weekend Crew  ").unwrap();

    assert_eq!(
        result,
        CreateFamilyResult::Created(family(42, 7, "Weekend Crew"))
    );
    assert_eq!(
        create_calls.borrow().as_slice(),
        &[(7, "Weekend Crew".to_string())]
    );
}

#[test]
fn create_family_rejects_empty_name() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_validation_error(
        service.create_family(7, "   ").map(|_| ()),
        "Family name cannot be empty",
    );
}

#[test]
fn create_family_returns_already_in_family_block() {
    let repo = FamilyRepo::default();
    repo.is_in_family.borrow_mut().insert(7, true);
    let service = FamilyService::new(Box::new(repo));

    let result = service.create_family(7, "Crew").unwrap();

    assert_eq!(result, CreateFamilyResult::AlreadyInFamily);
}

#[test]
fn create_family_returns_pending_invitation_block() {
    let repo = FamilyRepo::default();
    repo.pending_invitation
        .borrow_mut()
        .insert(7, Some(invitation(2, 1, "Other family")));
    let service = FamilyService::new(Box::new(repo));

    let result = service.create_family(7, "Crew").unwrap();

    assert_eq!(result, CreateFamilyResult::HasPendingInvitation);
}

#[test]
fn rename_family_trims_name_and_returns_updated_summary() {
    let repo = FamilyRepo::default();
    let rename_calls = Rc::clone(&repo.rename_calls);
    repo.rename_results.borrow_mut().insert((3, 7), true);
    let service = FamilyService::new(Box::new(repo));

    let result = service.rename_family(3, 7, "  New Name  ").unwrap();

    assert_eq!(result, family(3, 7, "New Name"));
    assert_eq!(
        rename_calls.borrow().as_slice(),
        &[(3, 7, "New Name".to_string())]
    );
}

#[test]
fn rename_family_rejects_empty_name() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_validation_error(
        service.rename_family(3, 7, "   ").map(|_| ()),
        "Family name cannot be empty",
    );
}

#[test]
fn rename_family_returns_not_found_when_family_does_not_exist() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.rename_family(3, 7, "New Name"),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn invite_member_requires_owner() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 99, "Crew")));
    let service = FamilyService::new(Box::new(repo));

    let result = service.invite_member_by_owner(7, 3, 11).unwrap();

    assert_eq!(result, InviteMemberResult::NotOwner);
}

#[test]
fn invite_member_returns_already_in_family_for_existing_member() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    repo.is_in_family.borrow_mut().insert(11, true);
    let service = FamilyService::new(Box::new(repo));

    let result = service.invite_member_by_owner(7, 3, 11).unwrap();

    assert_eq!(result, InviteMemberResult::AlreadyInFamily);
}

#[test]
fn invite_member_returns_pending_invitation_when_any_pending_invite_exists() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    repo.pending_invitation
        .borrow_mut()
        .insert(11, Some(invitation(99, 1, "Other")));
    let service = FamilyService::new(Box::new(repo));

    let result = service.invite_member_by_owner(7, 3, 11).unwrap();

    assert_eq!(result, InviteMemberResult::HasPendingInvitation);
}

#[test]
fn invite_member_returns_invited_when_repository_accepts_invitation() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    repo.invite_results.borrow_mut().insert((3, 11), true);
    let service = FamilyService::new(Box::new(repo));

    let result = service.invite_member_by_owner(7, 3, 11).unwrap();

    assert_eq!(
        result,
        InviteMemberResult::Invited {
            family_name: "Crew".to_string()
        }
    );
}

#[test]
fn invite_member_returns_not_found_when_repository_fails_to_record_invitation() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.invite_member_by_owner(7, 3, 11),
        Err(WishlistError::NotFound)
    );
}

#[test]
fn accept_invitation_returns_accepted_when_repository_confirms_it() {
    let repo = FamilyRepo::default();
    repo.accept_results.borrow_mut().insert((3, 11), true);
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.accept_invitation_checked(11, 3).unwrap(),
        AcceptInvitationResult::Accepted
    );
}

#[test]
fn accept_invitation_returns_already_in_family_when_user_has_active_membership() {
    let repo = FamilyRepo::default();
    repo.is_in_family.borrow_mut().insert(12, true);
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.accept_invitation_checked(12, 3).unwrap(),
        AcceptInvitationResult::AlreadyInFamily
    );
}

#[test]
fn accept_invitation_returns_invitation_not_found_when_no_record_exists() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.accept_invitation_checked(13, 3).unwrap(),
        AcceptInvitationResult::InvitationNotFound
    );
}

#[test]
fn decline_invitation_returns_declined_when_repository_confirms_it() {
    let repo = FamilyRepo::default();
    repo.decline_results.borrow_mut().insert((3, 11), true);
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.decline_invitation_checked(11, 3).unwrap(),
        DeclineInvitationResult::Declined
    );
}

#[test]
fn decline_invitation_returns_already_in_family_when_user_has_active_membership() {
    let repo = FamilyRepo::default();
    repo.is_in_family.borrow_mut().insert(12, true);
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.decline_invitation_checked(12, 3).unwrap(),
        DeclineInvitationResult::AlreadyInFamily
    );
}

#[test]
fn decline_invitation_returns_invitation_not_found_when_no_record_exists() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.decline_invitation_checked(13, 3).unwrap(),
        DeclineInvitationResult::InvitationNotFound
    );
}

#[test]
fn remove_member_returns_not_owner_when_actor_is_not_the_family_owner() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.remove_member_by_owner(99, 11).unwrap(),
        RemoveMemberResult::NotOwner
    );
}

#[test]
fn remove_member_returns_cannot_remove_self_when_owner_targets_themselves() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.remove_member_by_owner(7, 7).unwrap(),
        RemoveMemberResult::CannotRemoveSelf
    );
}

#[test]
fn remove_member_returns_removed_when_repository_confirms_removal() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    repo.remove_results.borrow_mut().insert((3, 11), true);
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.remove_member_by_owner(7, 11).unwrap(),
        RemoveMemberResult::Removed
    );
}

#[test]
fn remove_member_returns_member_not_found_when_user_is_not_in_family() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.remove_member_by_owner(7, 12).unwrap(),
        RemoveMemberResult::MemberNotFound
    );
}

#[test]
fn leave_family_returns_left_when_repository_confirms_departure() {
    let repo = FamilyRepo::default();
    repo.leave_results.borrow_mut().insert(11, true);
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.leave_family_checked(11).unwrap(),
        LeaveFamilyResult::Left
    );
}

#[test]
fn leave_family_returns_owner_cannot_leave_when_user_owns_the_family() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.leave_family_checked(7).unwrap(),
        LeaveFamilyResult::OwnerCannotLeave
    );
}

#[test]
fn leave_family_returns_not_in_family_when_user_has_no_membership() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.leave_family_checked(99).unwrap(),
        LeaveFamilyResult::NotInFamily
    );
}

#[test]
fn delete_owned_family_returns_deleted_when_repository_confirms_deletion() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    repo.delete_results.borrow_mut().insert((3, 7), true);
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.delete_owned_family(7).unwrap(),
        DeleteFamilyResult::Deleted
    );
}

#[test]
fn delete_owned_family_returns_not_owner_when_user_is_not_the_owner() {
    let repo = FamilyRepo::default();
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(
        service.delete_owned_family(99).unwrap(),
        DeleteFamilyResult::NotOwner
    );
}

#[test]
fn delete_owned_family_returns_not_found_when_repo_delete_fails() {
    let repo = FamilyRepo::default();
    repo.family_for_user
        .borrow_mut()
        .insert(7, Some(family(3, 7, "Crew")));
    // delete_results не содержит (3, 7) → repo вернёт false
    let service = FamilyService::new(Box::new(repo));

    assert_eq!(service.delete_owned_family(7), Err(WishlistError::NotFound));
}
