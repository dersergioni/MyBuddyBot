use crate::WishlistError;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FamilySummary {
    pub id: i64,
    pub owner_id: i64,
    pub name: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PendingInvitation {
    pub family_id: i64,
    pub owner_id: i64,
    pub family_name: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CreateFamilyResult {
    Created(FamilySummary),
    AlreadyInFamily,
    HasPendingInvitation,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CreateFamilyCheckResult {
    CanCreate,
    AlreadyInFamily,
    HasPendingInvitation,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum InviteMemberResult {
    Invited { family_name: String },
    NotOwner,
    AlreadyInFamily,
    HasPendingInvitation,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum AcceptInvitationResult {
    Accepted,
    AlreadyInFamily,
    InvitationNotFound,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DeclineInvitationResult {
    Declined,
    AlreadyInFamily,
    InvitationNotFound,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum RemoveMemberResult {
    Removed,
    NotOwner,
    CannotRemoveSelf,
    MemberNotFound,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LeaveFamilyResult {
    Left,
    OwnerCannotLeave,
    NotInFamily,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DeleteFamilyResult {
    Deleted,
    NotOwner,
}

pub trait FamilyRepository {
    fn create_family(&self, owner_id: i64, name: &str) -> Result<i64, WishlistError>;
    fn get_family_for_user(&self, user_id: i64) -> Result<Option<FamilySummary>, WishlistError>;
    fn get_family_members(&self, family_id: i64) -> Result<Vec<i64>, WishlistError>;
    fn can_browse_family_member(
        &self,
        viewer_user_id: i64,
        member_user_id: i64,
    ) -> Result<bool, WishlistError>;
    fn can_access_public_family_list(
        &self,
        viewer_user_id: i64,
        list_id: i64,
    ) -> Result<bool, WishlistError>;
    fn invite_member(&self, family_id: i64, member_user_id: i64) -> Result<bool, WishlistError>;
    fn remove_family_member(
        &self,
        family_id: i64,
        member_user_id: i64,
    ) -> Result<bool, WishlistError>;
    fn is_user_in_family(&self, user_id: i64) -> Result<bool, WishlistError>;
    fn delete_family(&self, family_id: i64, owner_id: i64) -> Result<bool, WishlistError>;
    fn leave_family(&self, member_user_id: i64) -> Result<bool, WishlistError>;
    fn rename_family(
        &self,
        family_id: i64,
        owner_id: i64,
        new_name: &str,
    ) -> Result<Option<FamilySummary>, WishlistError>;
    fn get_pending_invitation(
        &self,
        invited_user_id: i64,
    ) -> Result<Option<PendingInvitation>, WishlistError>;
    fn accept_invitation(
        &self,
        family_id: i64,
        invited_user_id: i64,
    ) -> Result<bool, WishlistError>;
    fn decline_invitation(
        &self,
        family_id: i64,
        invited_user_id: i64,
    ) -> Result<bool, WishlistError>;
}

pub struct FamilyService {
    repo: Box<dyn FamilyRepository>,
}

impl FamilyService {
    pub fn new(repo: Box<dyn FamilyRepository>) -> Self {
        Self { repo }
    }

    pub fn get_family_for_user(
        &self,
        user_id: i64,
    ) -> Result<Option<FamilySummary>, WishlistError> {
        self.repo.get_family_for_user(user_id)
    }

    pub fn get_family_members(&self, family_id: i64) -> Result<Vec<i64>, WishlistError> {
        self.repo.get_family_members(family_id)
    }

    pub fn can_browse_family_member(
        &self,
        viewer_user_id: i64,
        member_user_id: i64,
    ) -> Result<bool, WishlistError> {
        self.repo
            .can_browse_family_member(viewer_user_id, member_user_id)
    }

    pub fn can_access_public_family_list(
        &self,
        viewer_user_id: i64,
        list_id: i64,
    ) -> Result<bool, WishlistError> {
        self.repo
            .can_access_public_family_list(viewer_user_id, list_id)
    }

    pub fn get_pending_invitation(
        &self,
        invited_user_id: i64,
    ) -> Result<Option<PendingInvitation>, WishlistError> {
        self.repo.get_pending_invitation(invited_user_id)
    }

    pub fn create_family(
        &self,
        owner_id: i64,
        name: &str,
    ) -> Result<CreateFamilyResult, WishlistError> {
        let trimmed_name = name.trim();
        if trimmed_name.is_empty() {
            return Err(WishlistError::Validation(
                "Family name cannot be empty".to_string(),
            ));
        }

        match self.check_create_family(owner_id)? {
            CreateFamilyCheckResult::AlreadyInFamily => {
                return Ok(CreateFamilyResult::AlreadyInFamily);
            }
            CreateFamilyCheckResult::HasPendingInvitation => {
                return Ok(CreateFamilyResult::HasPendingInvitation);
            }
            CreateFamilyCheckResult::CanCreate => {}
        }

        let family_id = self.repo.create_family(owner_id, trimmed_name)?;
        Ok(CreateFamilyResult::Created(FamilySummary {
            id: family_id,
            owner_id,
            name: trimmed_name.to_string(),
        }))
    }

    pub fn check_create_family(
        &self,
        owner_id: i64,
    ) -> Result<CreateFamilyCheckResult, WishlistError> {
        if self.repo.is_user_in_family(owner_id)? {
            return Ok(CreateFamilyCheckResult::AlreadyInFamily);
        }

        if self.repo.get_pending_invitation(owner_id)?.is_some() {
            return Ok(CreateFamilyCheckResult::HasPendingInvitation);
        }

        Ok(CreateFamilyCheckResult::CanCreate)
    }

    pub fn rename_family(
        &self,
        family_id: i64,
        owner_id: i64,
        new_name: &str,
    ) -> Result<FamilySummary, WishlistError> {
        let trimmed_name = new_name.trim();
        if trimmed_name.is_empty() {
            return Err(WishlistError::Validation(
                "Family name cannot be empty".to_string(),
            ));
        }

        self.repo
            .rename_family(family_id, owner_id, trimmed_name)?
            .ok_or(WishlistError::NotFound)
    }

    pub fn invite_member_by_owner(
        &self,
        owner_id: i64,
        family_id: i64,
        member_user_id: i64,
    ) -> Result<InviteMemberResult, WishlistError> {
        let family = match self.repo.get_family_for_user(owner_id)? {
            Some(family) if family.owner_id == owner_id && family.id == family_id => family,
            _ => return Ok(InviteMemberResult::NotOwner),
        };

        if self.repo.is_user_in_family(member_user_id)? {
            return Ok(InviteMemberResult::AlreadyInFamily);
        }

        if self.repo.get_pending_invitation(member_user_id)?.is_some() {
            return Ok(InviteMemberResult::HasPendingInvitation);
        }

        if self.repo.invite_member(family_id, member_user_id)? {
            return Ok(InviteMemberResult::Invited {
                family_name: family.name,
            });
        }

        Err(WishlistError::NotFound)
    }

    pub fn accept_invitation_checked(
        &self,
        invited_user_id: i64,
        family_id: i64,
    ) -> Result<AcceptInvitationResult, WishlistError> {
        if self.repo.accept_invitation(family_id, invited_user_id)? {
            return Ok(AcceptInvitationResult::Accepted);
        }

        if self.repo.is_user_in_family(invited_user_id)? {
            return Ok(AcceptInvitationResult::AlreadyInFamily);
        }

        Ok(AcceptInvitationResult::InvitationNotFound)
    }

    pub fn decline_invitation_checked(
        &self,
        invited_user_id: i64,
        family_id: i64,
    ) -> Result<DeclineInvitationResult, WishlistError> {
        if self.repo.decline_invitation(family_id, invited_user_id)? {
            return Ok(DeclineInvitationResult::Declined);
        }

        if self.repo.is_user_in_family(invited_user_id)? {
            return Ok(DeclineInvitationResult::AlreadyInFamily);
        }

        Ok(DeclineInvitationResult::InvitationNotFound)
    }

    pub fn remove_member_by_owner(
        &self,
        owner_id: i64,
        member_user_id: i64,
    ) -> Result<RemoveMemberResult, WishlistError> {
        let family = match self.repo.get_family_for_user(owner_id)? {
            Some(family) if family.owner_id == owner_id => family,
            _ => return Ok(RemoveMemberResult::NotOwner),
        };

        if member_user_id == owner_id {
            return Ok(RemoveMemberResult::CannotRemoveSelf);
        }

        if self.repo.remove_family_member(family.id, member_user_id)? {
            return Ok(RemoveMemberResult::Removed);
        }

        Ok(RemoveMemberResult::MemberNotFound)
    }

    pub fn leave_family_checked(
        &self,
        member_user_id: i64,
    ) -> Result<LeaveFamilyResult, WishlistError> {
        if self.repo.leave_family(member_user_id)? {
            return Ok(LeaveFamilyResult::Left);
        }

        if let Some(family) = self.repo.get_family_for_user(member_user_id)?
            && family.owner_id == member_user_id
        {
            return Ok(LeaveFamilyResult::OwnerCannotLeave);
        }

        Ok(LeaveFamilyResult::NotInFamily)
    }

    pub fn delete_owned_family(&self, owner_id: i64) -> Result<DeleteFamilyResult, WishlistError> {
        let family = match self.repo.get_family_for_user(owner_id)? {
            Some(family) if family.owner_id == owner_id => family,
            _ => return Ok(DeleteFamilyResult::NotOwner),
        };

        if self.repo.delete_family(family.id, owner_id)? {
            return Ok(DeleteFamilyResult::Deleted);
        }

        Err(WishlistError::NotFound)
    }
}
