/*! State of the emulated keyboard and keys */

use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt;
use std::io;
use std::rc::Rc;
use std::string::FromUtf8Error;
    
use ::symbol::{ Symbol, Action };

use std::io::Write;
use std::iter::{ FromIterator, IntoIterator };

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    use ::util::c;
    use ::util::c::as_cstr;
    
    use std::ffi::CString;
    use std::os::raw::c_char;

    pub type CKeyState = c::Wrapped<KeyState>;

    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers

    #[no_mangle]
    pub extern "C"
    fn squeek_key_free(key: CKeyState) {
        unsafe { key.unwrap() }; // reference dropped
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
        return key.to_owned().keycode.unwrap_or(0u32);
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

        let symbol_name = match key.to_owned().symbol.action {
            Action::Submit { text: Some(text), .. } => {
                Some(
                    text.clone()
                        .into_string().expect("Bad symbol")
                )
            },
            _ => None,
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

        let symbol_name = match key.to_owned().symbol.action {
            Action::Submit { text: Some(text), .. } => {
                Some(
                    text.clone()
                        .into_string().expect("Bad symbol text")
                )
            },
            _ => None
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
    pub keycode: Option<u32>,
    pub symbol: Symbol,
}

/// Generates a mapping where each key gets a keycode, starting from 8
pub fn generate_keycodes<'a, C: IntoIterator<Item=&'a str>>(
    key_names: C
) -> HashMap<String, u32> {
    HashMap::from_iter(
        key_names.into_iter()
            .map(|name| String::from(name))
            .zip(8..)
    )
}

#[derive(Debug)]
pub enum FormattingError {
    Utf(FromUtf8Error),
    Format(io::Error),
}

impl fmt::Display for FormattingError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            FormattingError::Utf(e) => write!(f, "UTF: {}", e),
            FormattingError::Format(e) => write!(f, "Format: {}", e),
        }
    }
}

impl From<io::Error> for FormattingError {
    fn from(e: io::Error) -> Self {
        FormattingError::Format(e)
    }
}

/// Generates a de-facto single level keymap. TODO: actually drop second level
/// TODO: take in keysym->keycode mapping
pub fn generate_keymap(
    keystates: &HashMap::<String, Rc<RefCell<KeyState>>>
) -> Result<String, FormattingError> {
    let mut buf: Vec<u8> = Vec::new();
    writeln!(
        buf,
        "xkb_keymap {{

    xkb_keycodes \"squeekboard\" {{
        minimum = 8;
        maximum = 255;"
    )?;
    
    for (name, state) in keystates.iter() {
        if let Some(keycode) = state.borrow().keycode {
            write!(
                buf,
                "
        <{}> = {};",
                name,
                keycode
            )?;
        }
    }
    
    writeln!(
        buf,
        "
    }};
    
    xkb_symbols \"squeekboard\" {{

        name[Group1] = \"Letters\";
        name[Group2] = \"Numbers/Symbols\";"
    )?;
    
    for (name, state) in keystates.iter() {
        if let Some(_) = state.borrow().keycode {
            write!(
                buf,
                "
        key <{}> {{ [ {0} ] }};",
                name,
            )?;
        }
    }
    writeln!(
        buf,
        "
    }};

    xkb_types \"squeekboard\" {{

	type \"TWO_LEVEL\" {{
            modifiers = Shift;
            map[Shift] = Level2;
            level_name[Level1] = \"Base\";
            level_name[Level2] = \"Shift\";
	}};
    }};

    xkb_compatibility \"squeekboard\" {{
    }};
}};"
    )?;
    
    String::from_utf8(buf).map_err(FormattingError::Utf)
}
