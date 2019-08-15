use std::cell::RefCell;
use std::rc::Rc;
use std::vec::Vec;

use ::keyboard::*;
use ::float_ord::FloatOrd;
use ::symbol::*;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;

    use std::os::raw::c_void;
    use std::ptr;

    // The following defined in C

    #[repr(transparent)]
    pub struct UserData(*const c_void);
    
    /// The index in the relevant outline table
    #[repr(C)]
    #[derive(Clone, Debug)]
    pub struct OutlineRef(u32);

    /// Defined in eek-types.h
    #[repr(C)]
    #[derive(Clone, Debug)]
    pub struct Bounds {
        x: f64,
        y: f64,
        width: f64,
        height: f64
    }
    
    type ButtonCallback = unsafe extern "C" fn(button: *mut ::layout::Button, data: *mut UserData);

    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers
    
    #[no_mangle]
    pub extern "C"
    fn squeek_row_new(angle: i32) -> *mut ::layout::Row {
        Box::into_raw(Box::new(::layout::Row {
            buttons: Vec::new(),
            angle: angle,
        }))
    }
    
    /// Places a button into the row and returns a reference to it
    #[no_mangle]
    pub extern "C"
    fn squeek_row_create_button(
        row: *mut ::layout::Row,
        keycode: u32, oref: u32
    ) -> *mut ::layout::Button {
        let row = unsafe { &mut *row };
        let state: Rc<RefCell<::keyboard::KeyState>> = Rc::new(RefCell::new(
            ::keyboard::KeyState {
                pressed: false,
                locked: false,
                keycode: keycode,
                symbol: None,
            }
        ));
        row.buttons.push(Box::new(::layout::Button {
            oref: OutlineRef(oref),
            bounds: None,
            state: state,
        }));
        // Return the reference directly instead of a Box, it's not on the stack
        // It will live as long as the Vec
        let last_idx = row.buttons.len() - 1;
        // Caution: Box can't be returned directly,
        // so returning a reference to its innards
        row.buttons[last_idx].as_mut() as *mut ::layout::Button
    }
    
    /// Places a button into the row, copying its state,
    /// and returns a reference to it
    #[no_mangle]
    pub extern "C"
    fn squeek_row_create_button_with_state(
        row: *mut ::layout::Row,
        button: *const ::layout::Button,
    ) -> *mut ::layout::Button {
        let row = unsafe { &mut *row };
        let source = unsafe { &*button };
        row.buttons.push(Box::new(source.clone()));
        // Return the reference directly instead of a Box, it's not on the stack
        // It will live as long as the Vec
        let last_idx = row.buttons.len() - 1;
        // Caution: Box can't be returned directly,
        // so returning a reference to its innards directly
        row.buttons[last_idx].as_mut() as *mut ::layout::Button
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_row_set_angle(row: *mut ::layout::Row, angle: i32) {
        let row = unsafe { &mut *row };
        row.angle = angle;
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_row_get_angle(row: *const ::layout::Row) -> i32 {
        let row = unsafe { &*row };
        row.angle
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_row_contains(
        row: *mut ::layout::Row,
        needle: *const ::layout::Button,
    ) -> u32 {
        let row = unsafe { &mut *row };
        row.buttons.iter().position(
            // TODO: wrap Button properly in Rc; this comparison is unreliable
            |button| button.as_ref() as *const ::layout::Button == needle
        ).is_some() as u32
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_row_foreach(
        row: *mut ::layout::Row,
        callback: ButtonCallback,
        data: *mut UserData,
    ) {
        let row = unsafe { &mut *row };
        for button in row.buttons.iter_mut() {
            let button = button.as_mut() as *mut ::layout::Button;
            unsafe { callback(button, data) };
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_row_free(row: *mut ::layout::Row) {
        unsafe { Box::from_raw(row) }; // gets dropped
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_button_new(keycode: u32, oref: u32) -> *mut ::layout::Button {
        let state: Rc<RefCell<::keyboard::KeyState>> = Rc::new(RefCell::new(
            ::keyboard::KeyState {
                pressed: false,
                locked: false,
                keycode: keycode,
                symbol: None,
            }
        ));
        Box::into_raw(Box::new(::layout::Button {
            oref: OutlineRef(oref),
            bounds: None,
            state: state,
        }))
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_button_new_with_state(source: *mut ::layout::Button) -> *mut ::layout::Button {
        let source = unsafe { &*source };
        let button = Box::new(source.clone());
        Box::into_raw(button)
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_oref(button: *const ::layout::Button) -> u32 {
        let button = unsafe { &*button };
        button.oref.0
    }

    // Bounds transparently mapped to C, therefore no pointer needed

    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_bounds(button: *const ::layout::Button) -> Bounds {
        let button = unsafe { &*button };
        match &button.bounds {
            Some(bounds) => bounds.clone(),
            None => panic!("Button doesn't have any bounds yet"),
        }
    }
    
    /// Set bounds by consuming the value
    #[no_mangle]
    pub extern "C"
    fn squeek_button_set_bounds(button: *mut ::layout::Button, bounds: Bounds) {
        let button = unsafe { &mut *button };
        button.bounds = Some(bounds);
    }
    
    /// Borrow a new reference to key state. Doesn't need freeing
    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_key(
        button: *mut ::layout::Button
    ) -> ::keyboard::c::CKeyState {
        let button = unsafe { &mut *button };
        ::keyboard::c::CKeyState::wrap(button.state.clone())
    }
    
    /// Really should just return the label
    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_symbol(
        button: *const ::layout::Button,
    ) -> *const Symbol {
        let button = unsafe { &*button };
        let state = button.state.borrow();
        match state.symbol {
            Some(ref symbol) => symbol as *const Symbol,
            None => ptr::null(),
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_button_has_key(
        button: *const ::layout::Button,
        state: ::keyboard::c::CKeyState,
    ) -> u32 {
        let button = unsafe { &*button };
        let state = state.unwrap();
        let equal = Rc::ptr_eq(&button.state, &state);
        Rc::into_raw(state); // Prevent dropping
        equal as u32
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_button_print(button: *const ::layout::Button) {
        let button = unsafe { &*button };
        println!("{:?}", button);
    }
    
    /// More complex procedures and algoithms which span multiple modules
    mod procedures {
        use super::*;
        
        use std::convert::TryFrom;

        #[repr(transparent)]
        pub struct LevelKeyboard(*const c_void);

        #[no_mangle]
        extern "C" {
            fn eek_get_outline_size(
                keyboard: *const LevelKeyboard,
                outline: u32
            ) -> Bounds;
        }
        
        const BUTTON_SPACING: f64 = 4.0;
        
        /// Places each button in order, starting from 0 on the left,
        /// keeping the spacing.
        /// Sizes each button according to outline dimensions.
        /// Returns the width and height of the resulting row.
        #[no_mangle]
        pub extern "C"
        fn squeek_row_place_keys(
            row: *mut ::layout::Row,
            keyboard: *const LevelKeyboard,
        ) -> Bounds {
            let row = unsafe { &mut *row };

            // Size buttons
            for mut button in &mut row.buttons {
                button.bounds = Some(
                    unsafe { eek_get_outline_size(keyboard, button.oref.0) }
                );
            }

            // Place buttons
            let cumulative_width: f64 = row.buttons.iter().map(
                |button| button.bounds.as_ref().unwrap().width
            ).sum();

            let max_height = row.buttons.iter().map(
                |button| FloatOrd(
                    button.bounds.as_ref().unwrap().height
                )
            ).max()
                .unwrap_or(FloatOrd(0f64))
                .0;

            row.buttons.iter_mut().fold(0f64, |acc, button| {
                let mut bounds = button.bounds.as_mut().unwrap();
                bounds.x = acc;
                acc + bounds.width + BUTTON_SPACING
            });
            
            // Total size
            let total_width = match row.buttons.is_empty() {
                true => 0f64,
                false => {
                    let last_button = &row.buttons[row.buttons.len() - 1];
                    let bounds = last_button.bounds.as_ref().unwrap();
                    bounds.x + bounds.width
                },
            };

            Bounds {
                x: 0f64,
                y: 0f64,
                width: total_width,
                height: max_height,
            }
        }

        /// Finds a button sharing this state
        #[no_mangle]
        pub extern "C"
        fn squeek_row_find_key(
            row: *mut ::layout::Row,
            state: ::keyboard::c::CKeyState,
        ) -> *mut Button {
            let row = unsafe { &mut *row };
            let needle = state.unwrap();
            let found = row.buttons.iter_mut().find(
                |button| Rc::ptr_eq(&button.state, &needle)
            );
            Rc::into_raw(needle); // Prevent dropping
            match found {
                Some(button) => button.as_mut() as *mut Button,
                None => ptr::null_mut(),
            }
        }
    }
    
    #[cfg(test)]
    mod test {
        use super::*;
        
        #[test]
        fn button_has_key() {
            let button = squeek_button_new(0, 0);
            let state = squeek_button_get_key(button);
            assert_eq!(squeek_button_has_key(button, state.clone()), 1);
            let other_button = squeek_button_new(0, 0);
            assert_eq!(squeek_button_has_key(other_button, state.clone()), 0);
            let other_state = ::keyboard::c::squeek_key_new(0);
            assert_eq!(squeek_button_has_key(button, other_state), 0);
            let shared_button = squeek_button_new_with_state(button);
            assert_eq!(squeek_button_has_key(shared_button, state), 1);
        }
        
        #[test]
        fn row_has_button() {
            let row = squeek_row_new(0);
            let button = squeek_row_create_button(row, 0, 0);
            assert_eq!(squeek_row_contains(row, button), 1);
            let shared_button = squeek_row_create_button_with_state(row, button);
            assert_eq!(squeek_row_contains(row, shared_button), 1);
            let row = squeek_row_new(0);
            assert_eq!(squeek_row_contains(row, button), 0);
        }
    }
}

/// The graphical representation of a button
#[derive(Clone, Debug)]
pub struct Button {
    oref: c::OutlineRef,
    /// TODO: abolish Option, buttons should be created with bounds fully formed
    bounds: Option<c::Bounds>,
    /// current state, shared with other buttons
    pub state: Rc<RefCell<KeyState>>,
}

pub struct Row {
    buttons: Vec<Box<Button>>,
    angle: i32,
}
