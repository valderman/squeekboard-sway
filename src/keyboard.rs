/*! State of the emulated keyboard and keys */

use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt;
use std::io;
use std::rc::Rc;
use std::string::FromUtf8Error;
    
use ::action::Action;

use std::io::Write;
use std::iter::{ FromIterator, IntoIterator };

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    use ::util::c;

    pub type CKeyState = c::Wrapped<KeyState>;

    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers

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
        return key.to_owned().keycode.unwrap_or(0u32);
    }
}

#[derive(Debug, Clone)]
pub struct KeyState {
    pub pressed: bool,
    pub locked: bool,
    pub keycode: Option<u32>,
    /// Static description of what the key does when pressed or released
    pub action: Action,
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
        let state = state.borrow();
        if let Action::Submit { text: _, keys } = &state.action {
            match keys.len() {
                0 => eprintln!("Key {} has no keysyms", name),
                a => {
                    // TODO: don't ignore any keysyms
                    if a > 1 {
                        eprintln!("Key {} multiple keysyms", name);
                    }
                    write!(
                        buf,
                        "
        <{}> = {};",
                        keys[0].0,
                        state.keycode.unwrap()
                    )?;
                },
            };
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
        if let Action::Submit { text: _, keys } = &state.borrow().action {
            if let Some(keysym) = keys.iter().next() {
                write!(
                    buf,
                    "
        key <{}> {{ [ {0} ] }};",
                    keysym.0,
                )?;
            }
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
