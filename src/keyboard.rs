use std::vec::Vec;

use super::symbol;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    use ::util::c;
    use ::util::c::{ as_cstr, into_cstring };
    
    use std::cell::RefCell;
    use std::ffi::CString;
    use std::os::raw::c_char;
    use std::ptr;
    use std::rc::Rc;

    
    // The following defined in C
    #[no_mangle]
    extern "C" {
        fn eek_keysym_from_name(name: *const c_char) -> u32;
    }

    pub type CKeyState = c::Wrapped<KeyState>;

    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers
    
    // TODO: this will receive data from the filesystem,
    // so it should handle garbled strings in the future
    #[no_mangle]
    pub extern "C"
    fn squeek_key_new(keycode: u32) -> CKeyState {
        let state: Rc<RefCell<KeyState>> = Rc::new(RefCell::new(
            KeyState {
                pressed: false,
                locked: false,
                keycode: keycode,
                symbol: None,
            }
        ));
        CKeyState::wrap(state)
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_free(key: CKeyState) {
        unsafe { key.unwrap() }; // reference dropped
    }
    
    /// Compares pointers to the data
    #[no_mangle]
    pub extern "C"
    fn squeek_key_equal(key: CKeyState, key2: CKeyState) -> u32 {
        return Rc::ptr_eq(&key.clone_ref(), &key2.clone_ref()) as u32
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_is_pressed(key: CKeyState) -> u32 {
        //let key = unsafe { Rc::from_raw(key.0) };
        return key.to_owned().pressed as u32;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_set_pressed(key: CKeyState, pressed: u32) {
        let key = key.clone_ref();
        let mut key = key.borrow_mut();
        key.pressed = pressed != 0;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_is_locked(key: CKeyState) -> u32 {
        return key.to_owned().locked as u32;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_set_locked(key: CKeyState, locked: u32) {
        let key = key.clone_ref();
        let mut key = key.borrow_mut();
        key.locked = locked != 0;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_get_keycode(key: CKeyState) -> u32 {
        return key.to_owned().keycode as u32;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_set_keycode(key: CKeyState, code: u32) {
        let key = key.clone_ref();
        let mut key = key.borrow_mut();
        key.keycode = code;
    }
    
    // TODO: this will receive data from the filesystem,
    // so it should handle garbled strings in the future
    #[no_mangle]
    pub extern "C"
    fn squeek_key_add_symbol(
        key: CKeyState,
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

        use symbol::*;
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
        
        let key = key.clone_ref();
        let mut key = key.borrow_mut();

        if let Some(_) = key.symbol {
            eprintln!("Key {:?} already has a symbol defined", text);
            return;
        }

        key.symbol = Some(match element.to_bytes() {
            b"symbol" => Symbol {
                action: Action::Submit {
                    text: text,
                    keys: Vec::new(),
                },
                label: label,
                tooltip: tooltip,
            },
            _ => panic!("unsupported element type {:?}", element),
        });
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_key_get_symbol(key: CKeyState) -> *const symbol::Symbol {
        let key = key.clone_ref();
        let key = key.borrow();
        match key.symbol {
            // This pointer stays after the function exits,
            // so it must reference borrowed data and not any copy
            Some(ref symbol) => symbol as *const symbol::Symbol,
            None => ptr::null(),
        }
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_key_to_keymap_entry(
        key_name: *const c_char,
        key: CKeyState,
    ) -> *const c_char {
        let key_name = as_cstr(&key_name)
            .expect("Missing key name")
            .to_str()
            .expect("Bad key name");

        let symbol_name = match key.to_owned().symbol {
            Some(ref symbol) => match &symbol.action {
                symbol::Action::Submit { text: Some(text), .. } => {
                    Some(
                        text.clone()
                            .into_string().expect("Bad symbol")
                    )
                },
                _ => None
            },
            None => {
                eprintln!("Key {} has no symbol", key_name);
                None
            },
        };

        let inner = match symbol_name {
            Some(name) => format!("[ {} ]", name),
            _ => format!("[ ]"),
        };

        CString::new(format!("        key <{}> {{ {} }};\n", key_name, inner))
            .expect("Couldn't convert string")
            .into_raw()
    }
    
        #[no_mangle]
    pub extern "C"
    fn squeek_key_get_action_name(
        key_name: *const c_char,
        key: CKeyState,
    ) -> *const c_char {
        let key_name = as_cstr(&key_name)
            .expect("Missing key name")
            .to_str()
            .expect("Bad key name");

        let symbol_name = match key.to_owned().symbol {
            Some(ref symbol) => match &symbol.action {
                symbol::Action::Submit { text: Some(text), .. } => {
                    Some(
                        text.clone()
                            .into_string().expect("Bad symbol")
                    )
                },
                _ => None
            },
            None => {
                eprintln!("Key {} has no symbol", key_name);
                None
            },
        };

        let inner = match symbol_name {
            Some(name) => format!("[ {} ]", name),
            _ => format!("[ ]"),
        };

        CString::new(format!("        key <{}> {{ {} }};\n", key_name, inner))
            .expect("Couldn't convert string")
            .into_raw()
    }

}

#[derive(Debug, Clone)]
pub struct KeyState {
    pub pressed: bool,
    pub locked: bool,
    pub keycode: u32,
    // TODO: remove the optionality of a symbol
    pub symbol: Option<symbol::Symbol>,
}
