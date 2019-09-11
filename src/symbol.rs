/*! The symbol object, defining actions that the key can do when activated */

use std::ffi::CString;

/// Just defines some int->identifier mappings for convenience
#[derive(Debug, Clone)]
pub enum KeySym {
    Unknown = 0,
    Shift = 0xffe1,
}

impl KeySym {
    pub fn from_u32(num: u32) -> KeySym {
        match num {
            0xffe1 => KeySym::Shift,
            _ => KeySym::Unknown,
        }
    }
}

#[derive(Debug, Clone)]
pub struct XKeySym(pub u32);

/// Use to switch layouts
type Level = String;

/// Use to send modified keypresses
#[derive(Debug, Clone)]
pub enum Modifier {
    Control,
    Alt,
}

/// Action to perform on the keypress and, in reverse, on keyrelease
#[derive(Debug, Clone)]
pub enum Action {
    /// Switch to this view
    SetLevel(Level),
    /// Switch to a view and latch
    LockLevel {
        lock: Level,
        /// When unlocked by pressing it or emitting a key
        unlock: Level,
    },
    /// Set this modifier TODO: release?
    SetModifier(Modifier),
    /// Submit some text
    Submit {
        /// Text to submit with input-method
        text: Option<CString>,
        /// The key events this symbol submits when submitting text is not possible
        keys: Vec<XKeySym>,
    },
}

/// Contains a static description of a particular key's actions
#[derive(Debug, Clone)]
pub struct Symbol {
    /// The action that this key performs
    pub action: Action,
}
