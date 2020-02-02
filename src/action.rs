/*! The symbol object, defining actions that the key can do when activated */

use std::ffi::CString;

/// Name of the keysym
#[derive(Debug, Clone, PartialEq)]
pub struct KeySym(pub String);

/// Use to switch views
type View = String;

/// Use to send modified keypresses
#[derive(Debug, Clone, PartialEq)]
pub enum Modifier {
    Control,
    Alt,
}

/// Action to perform on the keypress and, in reverse, on keyrelease
#[derive(Debug, Clone, PartialEq)]
pub enum Action {
    /// Switch to this view
    SetView(View),
    /// Switch to a view and latch
    LockView {
        lock: View,
        /// When unlocked by pressing it or emitting a key
        unlock: View,
    },
    /// Set this modifier TODO: release?
    SetModifier(Modifier),
    /// Submit some text
    Submit {
        /// Text to submit with input-method
        text: Option<CString>,
        /// The key events this symbol submits when submitting text is not possible
        keys: Vec<KeySym>,
    },
    ShowPreferences,
}

impl Action {
    pub fn is_locked(&self, view_name: &str) -> bool {
        match self {
            Action::LockView { lock, unlock: _ } => lock == view_name,
            _ => false,
        }
    }
    pub fn is_active(&self, view_name: &str) -> bool {
        match self {
            Action::SetView(view) => view == view_name,
            Action::LockView { lock, unlock: _ } => lock == view_name,
            _ => false,
        }
    }
}
