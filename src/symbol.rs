use std::ffi::CString;


/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    
    use std::ffi::CStr;
    use std::os::raw::c_char;
    use std::ptr;
    
    // The following defined in C
    
    // Legacy; Will never be used in Rust as a bit field
    enum ModifierMask {
        Nothing = 0,
        Shift = 1,
    }
    
    // The following defined in Rust.
    
    // TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers
    // Symbols are owned by Rust and will move towards no C manipulation, so it may make sense not to wrap them
    #[no_mangle]
    pub extern "C"
    fn squeek_symbol_get_name(symbol: *const Symbol) -> *const c_char {
        let symbol = unsafe { &*symbol };
        match &symbol.action {
            Action::Submit { text: Some(text), .. } => text.as_ptr(),
            _ => ptr::null(),
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_symbol_get_label(symbol: *const Symbol) -> *const c_char {
        let symbol = unsafe { &*symbol };
        match &symbol.label {
            Label::Text(text) => text.as_ptr(),
            // returning static strings to C is a bit cumbersome
            Label::IconName(_) => unsafe {
                CStr::from_bytes_with_nul_unchecked(b"icon\0")
            }.as_ptr(),
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_symbol_get_icon_name(symbol: *const Symbol) -> *const c_char {
        let symbol = unsafe { &*symbol };
        match &symbol.label {
            Label::Text(_) => ptr::null(),
            Label::IconName(name) => name.as_ptr(),
        }
    }
    
    // Legacy; throw away
    #[no_mangle]
    pub extern "C"
    fn squeek_symbol_get_modifier_mask(symbol: *const Symbol) -> u32 {
        let symbol = unsafe { &*symbol };
        (match &symbol.action {
            Action::SetLevel(1) => ModifierMask::Shift,
            _ => ModifierMask::Nothing,
        }) as u32
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_symbol_print(symbol: *const Symbol) {
        let symbol = unsafe { &*symbol };
        println!("{:?}", symbol);
    }
}

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

#[derive(Debug, Clone)]
pub enum Label {
    /// Text used to display the symbol
    Text(CString),
    /// Icon name used to render the symbol
    IconName(CString),
}

/// Use to switch layouts
type Level = u8;

/// Use to send modified keypresses
#[derive(Debug, Clone)]
pub enum Modifier {
    Control,
    Alt,
}

/// Action to perform on the keypress and, in reverse, on keyrelease
#[derive(Debug, Clone)]
pub enum Action {
    /// Switch to this level TODO: reverse?
    SetLevel(Level),
    /// Set this modifier TODO: release?
    SetModifier(Modifier),
    /// Submit some text
    Submit {
        /// orig: Canonical name of the symbol
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
    /// Label to display to the user
    pub label: Label,
    // FIXME: is it used?
    pub tooltip: Option<CString>,
}
