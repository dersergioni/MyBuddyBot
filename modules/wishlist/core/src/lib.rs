pub mod families;
pub mod lists;
pub mod service;
pub mod wishes;

use std::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum WishlistError {
    Validation(String),
    NotFound,
    AccessDenied,
    Storage(String),
}

impl fmt::Display for WishlistError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Validation(message) => write!(f, "Validation error: {message}"),
            Self::NotFound => write!(f, "Not found"),
            Self::AccessDenied => write!(f, "Access denied"),
            Self::Storage(message) => write!(f, "Storage error: {message}"),
        }
    }
}

impl std::error::Error for WishlistError {}
