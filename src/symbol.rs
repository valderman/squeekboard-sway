use std::boxed::Box;
use std::ffi::CString;


/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;

    use std::ffi::CStr;
    use std::os::raw::c_char;
    use std::ptr;
    use std::str::Utf8Error;

    
    fn as_str(s: &*const c_char) -> Result<Option<&str>, Utf8Error> {
        if s.is_null() {
            Ok(None)
        } else {
            unsafe {CStr::from_ptr(*s)}
                .to_str()
                .map(Some)
        }
    }
    
    fn as_cstr(s: &*const c_char) -> Option<&CStr> {
        if s.is_null() {
            None
        } else {
            Some(unsafe {CStr::from_ptr(*s)})
        }
    }

    fn into_cstring(s: *const c_char) -> Result<Option<CString>, std::ffi::NulError> {
        if s.is_null() {
            Ok(None)
        } else {
            CString::new(
                unsafe {CStr::from_ptr(s)}.to_bytes()
            ).map(Some)
        }
    }
    
    #[cfg(test)]
    mod tests {
        use super::*;
        
        #[test]
        fn test_null_cstring() {
            assert_eq!(into_cstring(ptr::null()), Ok(None))
        }
        
        #[test]
        fn test_null_str() {
            assert_eq!(as_str(&ptr::null()), Ok(None))
        }
    }
    
    // The following defined in C
    
    #[no_mangle]
    extern "C" {
        fn eek_keysym_from_name(name: *const c_char) -> u32;
    }

    // Legacy; Will never be used in Rust as a bit field
    enum ModifierMask {
        Nothing = 0,
        Shift = 1,
    }
    
    // The following defined in Rust.
    
    // TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers
    // Symbols are owned by Rust and will move towards no C manipulation, so it may make sense not to wrap them
    
    // TODO: this will receive data from the filesystem,
    // so it should handle garbled strings in the future
    #[no_mangle]
    pub extern "C"
    fn squeek_symbols_add(
        v: *mut Vec<Symbol>,
        element: *const c_char,
        text_raw: *const c_char, keyval: u32,
        label: *const c_char, icon: *const c_char,
        tooltip: *const c_char,
    ) {
        let element = as_cstr(&element)
            .expect("Missing element name");

        let text = into_cstring(text_raw)
            .unwrap_or_else(|e| {
                eprintln!("Text unreadable: {}", e);
                None
            })
            .and_then(|text| {
                if text.as_bytes() == b"" {
                    None
                } else {
                    Some(text)
                }
            });

        let icon = into_cstring(icon)
            .unwrap_or_else(|e| {
                eprintln!("Icon name unreadable: {}", e);
                None
            });

        // Only read label if there's no icon
        let label = match icon {
            Some(icon) => Label::IconName(icon),
            None => Label::Text(
                into_cstring(label)
                    .unwrap_or_else(|e| {
                        eprintln!("Label unreadable: {}", e);
                        Some(CString::new(" ").unwrap())
                    })
                    .unwrap_or_else(|| {
                        eprintln!("Label missing");
                        CString::new(" ").unwrap()
                    })
            ),
        };

        let tooltip = into_cstring(tooltip)
            .unwrap_or_else(|e| {
                eprintln!("Tooltip unreadable: {}", e);
                None
            });
        
        let symbol = match element.to_bytes() {
            b"symbol" => Symbol {
                action: Action::Submit {
                    text: text,
                    keys: Vec::new(),
                },
                label: label,
                tooltip: tooltip,
            },
            b"keysym" => {
                let keysym = XKeySym(
                    if keyval == 0 {
                        unsafe { eek_keysym_from_name(text_raw) }
                    } else {
                        keyval
                    }
                );
                Symbol {
                    action: match KeySym::from_u32(keysym.0) {
                        KeySym::Shift => Action::SetLevel(1),
                        _ => Action::Submit {
                            text: text,
                            keys: vec![keysym],
                        }
                    },
                    label: label,
                    tooltip: tooltip,
                }
            },
            _ => panic!("unsupported element type {:?}", element),
        };
        
        let v = unsafe { &mut *v };
        v.push(symbol);
    }
    
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
    
    #[no_mangle]
    pub extern "C"
    fn squeek_symbols_new() -> *const Vec<Symbol> {
        Box::into_raw(Box::new(Vec::new()))
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_symbols_get(v: *mut Vec<Symbol>, index: u32) -> *const Symbol {
        let v = unsafe { &mut *v };
        let index = index as usize;
        &v[
            if index < v.len() { index } else { 0 }
        ] as *const Symbol
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_symbols_free(symbols: *mut Vec<Symbol>) {
        unsafe { Box::from_raw(symbols) }; // Will just get dropped, together with the contents
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_to_keymap_entry(
        key_name: *const c_char,
        symbols: *const Vec<Symbol>,
    ) -> *const c_char {
        let key_name = as_cstr(&key_name)
            .expect("Missing key name")
            .to_str()
            .expect("Bad key name");
        let symbols = unsafe { &*symbols };
        let symbol_names = symbols.iter()
            .map(|symbol| {
                match &symbol.action {
                    Action::Submit { text: Some(text), .. } => {
                        Some(
                            text.clone()
                                .into_string().expect("Bad symbol")
                        )
                    },
                    _ => None
                }
            })
            .collect::<Vec<_>>();

        let inner = match symbol_names.len() {
            1 => match &symbol_names[0] {
                Some(name) => format!("[ {} ]", name),
                _ => format!("[ ]"),
            },
            4 => {
                let first = match (&symbol_names[0], &symbol_names[1]) {
                    (Some(left), Some(right)) => format!("{}, {}", left, right),
                    _ => format!(""),
                };
                let second = match (&symbol_names[2], &symbol_names[3]) {
                    (Some(left), Some(right)) => format!("{}, {}", left, right),
                    _ => format!(""),
                };
                format!("[ {} ], [ {} ]", first, second)
            },
            _ => panic!("Unsupported number of symbols"),
        };

        CString::new(format!("        key <{}> {{ {} }};\n", key_name, inner))
            .expect("Couldn't convert string")
            .into_raw()
    }
}

/// Just defines some int->identifier mappings for convenience
#[derive(Debug)]
enum KeySym {
    Unknown = 0,
    Shift = 0xffe1,
}

impl KeySym {
    fn from_u32(num: u32) -> KeySym {
        match num {
            0xffe1 => KeySym::Shift,
            _ => KeySym::Unknown,
        }
    }
}

#[derive(Debug)]
pub struct XKeySym(u32);

#[derive(Debug)]
pub enum Label {
    /// Text used to display the symbol
    Text(CString),
    /// Icon name used to render the symbol
    IconName(CString),
}

/// Use to switch layouts
type Level = u8;

/// Use to send modified keypresses
#[derive(Debug)]
pub enum Modifier {
    Control,
    Alt,
}

/// Action to perform on the keypress and, in reverse, on keyrelease
#[derive(Debug)]
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
#[derive(Debug)]
pub struct Symbol {
    /// The action that this key performs
    action: Action,
    /// Label to display to the user
    label: Label,
    // FIXME: is it used?
    tooltip: Option<CString>,
}
