use std::cell::RefCell;
use std::rc::Rc;

use ::symbol::*;
use ::keyboard::*;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;

    use std::ptr;

    // The following defined in C
    
    /// The index in the relevant outline table
    #[repr(C)]
    #[derive(Clone)]
    pub struct OutlineRef(u32);
    
    /// Defined in eek-types.h
    #[repr(C)]
    #[derive(Clone)]
    pub struct Bounds {
        x: f64,
        y: f64,
        width: f64,
        height: f64
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
    }
}

/// The graphical representation of a button
#[derive(Clone)]
pub struct Button {
    oref: c::OutlineRef,
    /// TODO: abolish Option, buttons should be created with bounds fully formed
    bounds: Option<c::Bounds>,
    /// current state, shared with other buttons
    pub state: Rc<RefCell<KeyState>>,
}
