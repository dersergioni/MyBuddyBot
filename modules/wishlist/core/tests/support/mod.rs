#![allow(dead_code)]

use wishlist_core::WishlistError;
use wishlist_core::families::{FamilySummary, PendingInvitation};
use wishlist_core::lists::{ListSummary, PublicListSummary};
use wishlist_core::wishes::Wish;

pub fn family(id: i64, owner_id: i64, name: &str) -> FamilySummary {
    FamilySummary {
        id,
        owner_id,
        name: name.to_string(),
    }
}

pub fn invitation(family_id: i64, owner_id: i64, family_name: &str) -> PendingInvitation {
    PendingInvitation {
        family_id,
        owner_id,
        family_name: family_name.to_string(),
    }
}

pub fn list(id: i64, name: &str, is_private: bool) -> ListSummary {
    ListSummary {
        id,
        name: name.to_string(),
        is_private,
    }
}

pub fn public_list(id: i64, name: &str) -> PublicListSummary {
    PublicListSummary {
        id,
        name: name.to_string(),
    }
}

pub fn wish(id: i64, title: &str, reserved_by: Option<i64>) -> Wish {
    Wish {
        id,
        title: title.to_string(),
        url: None,
        priority: None,
        notes: None,
        reserved_by,
    }
}

pub fn assert_validation_error(result: Result<(), WishlistError>, expected: &str) {
    assert_eq!(result, Err(WishlistError::Validation(expected.to_string())));
}
