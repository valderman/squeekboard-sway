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
    pub struct Point {
        pub x: f64,
        pub y: f64,
    }

    /// Defined in eek-types.h
    #[repr(C)]
    #[derive(Clone, Debug)]
    pub struct Bounds {
        pub x: f64,
        pub y: f64,
        pub width: f64,
        pub height: f64
    }
    
    type ButtonCallback = unsafe extern "C" fn(button: *mut ::layout::Button, data: *mut UserData);
    type RowCallback = unsafe extern "C" fn(row: *mut ::layout::Row, data: *mut UserData);

    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers
    
    #[no_mangle]
    pub extern "C"
    fn squeek_view_new(bounds: Bounds) -> *mut ::layout::View {
        Box::into_raw(Box::new(::layout::View {
            rows: Vec::new(),
            bounds: bounds,
        }))
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_view_get_bounds(view: *const ::layout::View) -> Bounds {
        unsafe { &*view }.bounds.clone()
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_view_set_bounds(view: *mut ::layout::View, bounds: Bounds) {
        unsafe { &mut *view }.bounds = bounds;
    }
    
    /// Places a row into the view and returns a reference to it
    #[no_mangle]
    pub extern "C"
    fn squeek_view_create_row(
        view: *mut ::layout::View,
        angle: i32,
    ) -> *mut ::layout::Row {
        let view = unsafe { &mut *view };

        view.rows.push(Box::new(::layout::Row::new(angle)));
        // Return the reference directly instead of a Box, it's not on the stack
        // It will live as long as the Vec
        let last_idx = view.rows.len() - 1;
        // Caution: Box can't be returned directly,
        // so returning a reference to its innards
        view.rows[last_idx].as_mut() as *mut ::layout::Row
    }
    
        #[no_mangle]
    pub extern "C"
    fn squeek_view_foreach(
        view: *mut ::layout::View,
        callback: RowCallback,
        data: *mut UserData,
    ) {
        let view = unsafe { &mut *view };
        for row in view.rows.iter_mut() {
            let row = row.as_mut() as *mut ::layout::Row;
            unsafe { callback(row, data) };
        }
    }

    
    #[no_mangle]
    pub extern "C"
    fn squeek_row_new(angle: i32) -> *mut ::layout::Row {
        Box::into_raw(Box::new(::layout::Row::new(angle)))
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
    fn squeek_row_get_bounds(row: *const ::layout::Row) -> Bounds {
        let row = unsafe { &*row };
        match &row.bounds {
            Some(bounds) => bounds.clone(),
            None => panic!("Row doesn't have any bounds yet"),
        }
    }

    /// Set bounds by consuming the value
    #[no_mangle]
    pub extern "C"
    fn squeek_row_set_bounds(row: *mut ::layout::Row, bounds: Bounds) {
        let row = unsafe { &mut *row };
        row.bounds = Some(bounds);
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
    
    /// Entry points for more complex procedures and algoithms which span multiple modules
    mod procedures {
        use super::*;
        
        #[repr(transparent)]
        pub struct LevelKeyboard(*const c_void);

        #[no_mangle]
        extern "C" {
            fn eek_get_outline_size(
                keyboard: *const LevelKeyboard,
                outline: u32
            ) -> Bounds;

            /// Checks if point falls within bounds,
            /// which are relative to origin and rotated by angle (I think)
            fn eek_are_bounds_inside (bounds: Bounds,
                point: Point,
                origin: Point,
                angle: i32
            ) -> u32;
        }
        
        fn squeek_buttons_get_outlines(
            buttons: &Vec<Box<Button>>,
            keyboard: *const LevelKeyboard,
        ) -> Vec<Bounds> {
            buttons.iter().map(|button| {
                unsafe { eek_get_outline_size(keyboard, button.oref.0) }
            }).collect()
        }
        
        /// Places each button in order, starting from 0 on the left,
        /// keeping the spacing.
        /// Sizes each button according to outline dimensions.
        /// Sets the row size correctly
        #[no_mangle]
        pub extern "C"
        fn squeek_row_place_buttons(
            row: *mut ::layout::Row,
            keyboard: *const LevelKeyboard,
        ) {
            let row = unsafe { &mut *row };
            
            let sizes = squeek_buttons_get_outlines(&row.buttons, keyboard);
            
            row.place_buttons_with_sizes(sizes);
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
        
        #[no_mangle]
        pub extern "C"
        fn squeek_row_find_button_by_position(
            row: *mut Row, point: Point, origin: Point
        ) -> *mut Button {
            let row = unsafe { &mut *row };
            let row_bounds = row.bounds
                .as_ref().expect("Missing bounds on row");
            let origin = Point {
                x: origin.x + row_bounds.x,
                y: origin.y + row_bounds.y,
            };
            let angle = row.angle;
            let result = row.buttons.iter_mut().find(|button| {
                let bounds = button.bounds
                    .as_ref().expect("Missing bounds on button")
                    .clone();
                let point = point.clone();
                let origin = origin.clone();
                unsafe {
                    eek_are_bounds_inside(bounds, point, origin, angle) == 1
                }
            });
            
            match result {
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


const BUTTON_SPACING: f64 = 4.0;

/// The graphical representation of a row of buttons
pub struct Row {
    buttons: Vec<Box<Button>>,
    /// Angle is not really used anywhere...
    angle: i32,
    bounds: Option<c::Bounds>,
}

impl Row {
    fn new(angle: i32) -> Row {
        Row {
            buttons: Vec::new(),
            angle: angle,
            bounds: None,
        }
    }
    fn place_buttons_with_sizes(&mut self, outlines: Vec<c::Bounds>) {
        let max_height = outlines.iter().map(
            |bounds| FloatOrd(bounds.height)
        ).max()
            .unwrap_or(FloatOrd(0f64))
            .0;

        let mut acc = 0f64;
        let x_offsets: Vec<f64> = outlines.iter().map(|outline| {
            acc += outline.x; // account for offset outlines
            let out = acc;
            acc += outline.width + BUTTON_SPACING;
            out
        }).collect();

        let total_width = acc;

        for ((mut button, outline), offset)
            in &mut self.buttons
                .iter_mut()
                .zip(outlines)
                .zip(x_offsets) {
            button.bounds = Some(c::Bounds {
                x: offset,
                ..outline
            });
        }
        
        let old_row_bounds = self.bounds.as_ref().unwrap().clone();
        self.bounds = Some(c::Bounds {
            // FIXME: do centering of each row based on keyboard dimensions,
            // one level up the iterators
            // now centering by comparing previous width to the new,
            // calculated one
            x: (old_row_bounds.width - total_width) / 2f64,
            width: total_width,
            height: max_height,
            ..old_row_bounds
        });
    }
}

pub struct View {
    bounds: c::Bounds,
    rows: Vec<Box<Row>>,
}
