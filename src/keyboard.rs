use std::vec::Vec;

use super::symbol;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    use ::util::c::{ as_cstr, into_cstring };
    
    use std::ffi::CString;
    use std::os::raw::c_char;
    
    // The following defined in C
    #[no_mangle]
    extern "C" {
        fn eek_keysym_from_name(name: *const c_char) -> u32;
    }


    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers
    
    // TODO: this will receive data from the filesystem,
    // so it should handle garbled strings in the future
    #[no_mangle]
    pub extern "C"
    fn squeek_key_new(keycode: u32) -> *mut KeyState {
        Box::into_raw(Box::new(
            KeyState {
                pressed: false,
                locked: false,
                keycode: keycode,
                symbols: Vec::new(),
            }
        ))
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_free(key: *mut KeyState) {
        unsafe { Box::from_raw(key) }; // gets dropped
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_is_pressed(key: *const KeyState) -> u32 {
        let key = unsafe { &*key };
        return key.pressed as u32;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_set_pressed(key: *mut KeyState, pressed: u32) {
        let key = unsafe { &mut *key };
        key.pressed = pressed != 0;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_is_locked(key: *const KeyState) -> u32 {
        let key = unsafe { &*key };
        return key.locked as u32;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_set_locked(key: *mut KeyState, locked: u32) {
        let key = unsafe { &mut *key };
        key.locked = locked != 0;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_get_keycode(key: *const KeyState) -> u32 {
        let key = unsafe { &*key };
        return key.keycode as u32;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_key_set_keycode(key: *mut KeyState, code: u32) {
        let key = unsafe { &mut *key };
        key.keycode = code;
    }
    
    // TODO: this will receive data from the filesystem,
    // so it should handle garbled strings in the future
    #[no_mangle]
    pub extern "C"
    fn squeek_key_add_symbol(
        key: *mut KeyState,
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

        let key = unsafe { &mut *key };
        
        if key.symbols.len() > 0 {
            eprintln!("Key {:?} already has a symbol defined", text);
            return;
        }


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
        
        key.symbols.push(symbol);
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_key_get_symbol(
        key: *const KeyState, index: u32
    ) -> *const symbol::Symbol {
        let key = unsafe { &*key };
        let index = index as usize;
        &key.symbols[
            if index < key.symbols.len() { index } else { 0 }
        ] as *const symbol::Symbol
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_key_to_keymap_entry(
        key_name: *const c_char,
        key: *const KeyState,
    ) -> *const c_char {
        let key_name = as_cstr(&key_name)
            .expect("Missing key name")
            .to_str()
            .expect("Bad key name");

        let key = unsafe { &*key };
        let symbol_names = key.symbols.iter()
            .map(|symbol| {
                match &symbol.action {
                    symbol::Action::Submit { text: Some(text), .. } => {
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
            _ => panic!("Unsupported number of symbols: {}", symbol_names.len()),
        };

        CString::new(format!("        key <{}> {{ {} }};\n", key_name, inner))
            .expect("Couldn't convert string")
            .into_raw()
    }
}

#[derive(Debug)]
pub struct KeyState {
    pressed: bool,
    locked: bool,
    keycode: u32,
    symbols: Vec<symbol::Symbol>,
}
