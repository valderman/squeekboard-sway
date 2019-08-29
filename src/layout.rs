/**
 * Layout-related data.
 * 
 * The `View` contains `Row`s and each `Row` contains `Button`s.
 * They carry data relevant to their positioning only,
 * except the Button, which also carries some data
 * about its appearance and function.
 * 
 * The layout is determined bottom-up, by measuring `Button` sizes,
 * deriving `Row` sizes from them, and then centering them within the `View`.
 * 
 * That makes the `View` position immutable,
 * and therefore different than the other positions.
 * 
 * Note that it might be a better idea
 * to make `View` position depend on its contents,
 * and let the renderer scale and center it within the widget.
 */

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
    
    impl Bounds {
        pub fn zero() -> Bounds {
            Bounds { x: 0f64, y: 0f64, width: 0f64, height: 0f64 }
        }
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
        button: *const ::layout::Button
    ) -> ::keyboard::c::CKeyState {
        let button = unsafe { &*button };
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
        let state = state.clone_ref();
        let equal = Rc::ptr_eq(&button.state, &state);
        equal as u32
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_button_print(button: *const ::layout::Button) {
        let button = unsafe { &*button };
        println!("{:?}", button);
    }
    
    /// Entry points for more complex procedures and algoithms which span multiple modules
    pub mod procedures {
        use super::*;
        
        #[repr(transparent)]
        pub struct LevelKeyboard(*const c_void);
        
        #[repr(C)]
        #[derive(PartialEq, Debug)]
        pub struct ButtonPlace {
            row: *const Row,
            button: *const Button,
        }

        #[no_mangle]
        extern "C" {
            fn eek_get_outline_size(
                keyboard: *const LevelKeyboard,
                outline: u32
            ) -> Bounds;

            /// Checks if point falls within bounds,
            /// which are relative to origin and rotated by angle (I think)
            pub fn eek_are_bounds_inside (bounds: Bounds,
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
        /// Places each row in order, starting from 0 on the top,
        /// keeping the spacing.
        /// Sets button and row sizes according to their contents.
        #[no_mangle]
        pub extern "C"
        fn squeek_view_place_contents(
            view: *mut ::layout::View,
            keyboard: *const LevelKeyboard, // source of outlines
        ) {
            let view = unsafe { &mut *view };
            
            let sizes: Vec<Vec<Bounds>> = view.rows.iter().map(|row|
                squeek_buttons_get_outlines(&row.buttons, keyboard)
            ).collect();

            view.place_buttons_with_sizes(sizes);
        }

        fn squeek_row_contains(row: &Row, needle: *const Button) -> bool {
            row.buttons.iter().position(
                // TODO: wrap Button properly in Rc; this comparison is unreliable
                |button| button.as_ref() as *const ::layout::Button == needle
            ).is_some()
        }
        
        #[no_mangle]
        pub extern "C"
        fn squeek_view_get_row(
            view: *mut View,
            needle: *const ::layout::Button,
        ) -> *mut Row {
            let view = unsafe { &mut *view };
            let result = view.rows.iter_mut().find(|row| {
                squeek_row_contains(row, needle)
            });
            match result {
                Some(row) => row.as_mut() as *mut Row,
                None => ptr::null_mut(),
            }
        }

        #[no_mangle]
        pub extern "C"
        fn squeek_view_find_key(
            view: *const View,
            needle: ::keyboard::c::CKeyState,
        ) -> ButtonPlace {
            let view = unsafe { &*view };
            let state = needle.clone_ref();
            
            let paths = ::layout::procedures::find_key_paths(view, &state);

            // Can only return 1 entry back to C
            let (row, button) = match paths.get(0) {
                Some((row, button)) => (
                    row.as_ref() as *const Row,
                    button.as_ref() as *const Button,
                ),
                None => ( ptr::null(), ptr::null() ),
            };
            ButtonPlace { row, button }
        }


        #[no_mangle]
        pub extern "C"
        fn squeek_view_find_button_by_position(
            view: *mut View, point: Point
        ) -> *mut Button {
            let view = unsafe { &mut *view };
            let result = view.find_button_by_position(point);
            match result {
                Some(button) => button.as_mut() as *mut Button,
                None => ptr::null_mut(),
            }
        }
        
        #[cfg(test)]
        mod test {
            use super::*;

            #[test]
            fn row_has_button() {
                let mut row = Row::new(0);
                let button = squeek_row_create_button(&mut row as *mut Row, 0, 0);
                assert_eq!(squeek_row_contains(&row, button), true);
                let shared_button = squeek_row_create_button_with_state(
                    &mut row as *mut Row,
                    button
                );
                assert_eq!(squeek_row_contains(&row, shared_button), true);
                let row = Row::new(0);
                assert_eq!(squeek_row_contains(&row, button), false);
            }

            #[test]
            fn view_has_button() {
                let state = Rc::new(RefCell::new(::keyboard::KeyState {
                    pressed: false,
                    locked: false,
                    keycode: 0,
                    symbol: None,
                }));
                let state_clone = ::keyboard::c::CKeyState::wrap(state.clone());

                let button = Box::new(Button {
                    oref: OutlineRef(0),
                    bounds: None,
                    state: state,
                });
                let button_ptr = button.as_ref() as *const Button;
                
                let row = Box::new(Row {
                    buttons: vec!(button),
                    angle: 0,
                    bounds: None
                });
                let row_ptr = row.as_ref() as *const Row;

                let view = View {
                    bounds: Bounds {
                        x: 0f64, y: 0f64,
                        width: 0f64, height: 0f64
                    },
                    rows: vec!(row),
                };

                assert_eq!(
                    squeek_view_find_key(
                        &view as *const View,
                        state_clone.clone(),
                    ),
                    ButtonPlace {
                        row: row_ptr,
                        button: button_ptr,
                    }
                );

                let view = View {
                    bounds: Bounds {
                        x: 0f64, y: 0f64,
                        width: 0f64, height: 0f64
                    },
                    rows: Vec::new(),
                };
                assert_eq!(
                    squeek_view_find_key(
                        &view as *const View,
                        state_clone.clone()
                    ),
                    ButtonPlace {
                        row: ptr::null(),
                        button: ptr::null(),
                    }
                );
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
    }
}

#[derive(Debug)]
pub struct Size {
    pub width: f64,
    pub height: f64,
}

/// The graphical representation of a button
#[derive(Clone, Debug)]
pub struct Button {
    oref: c::OutlineRef,
    /// TODO: abolish Option, buttons should be created with bounds fully formed
    /// Position relative to some origin (i.e. parent/row)
    bounds: Option<c::Bounds>,
    /// current state, shared with other buttons
    pub state: Rc<RefCell<KeyState>>,
}

// FIXME: derive from the style/margin/padding
const BUTTON_SPACING: f64 = 4.0;
const ROW_SPACING: f64 = 7.0;

/// The graphical representation of a row of buttons
pub struct Row {
    buttons: Vec<Box<Button>>,
    /// Angle is not really used anywhere...
    angle: i32,
    /// Position relative to some origin (i.e. parent/view origin)
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
    
    fn last(positions: &Vec<c::Bounds>) -> Option<&c::Bounds> {
        let len = positions.len();
        match len {
            0 => None,
            l => Some(&positions[l - 1])
        }
    }
    
    fn calculate_button_positions(outlines: Vec<c::Bounds>)
        -> Vec<c::Bounds>
    {
        let mut x_offset = 0f64;
        outlines.iter().map(|outline| {
            x_offset += outline.x; // account for offset outlines
            let position = c::Bounds {
                x: x_offset,
                ..outline.clone()
            };
            x_offset += outline.width + BUTTON_SPACING;
            position
        }).collect()
    }
    
    fn calculate_row_size(positions: &Vec<c::Bounds>) -> Size {
        let max_height = positions.iter().map(
            |bounds| FloatOrd(bounds.height)
        ).max()
            .unwrap_or(FloatOrd(0f64))
            .0;
        
        let total_width = match Row::last(positions) {
            Some(position) => position.x + position.width,
            None => 0f64,
        };
        Size { width: total_width, height: max_height }
    }

    /// Finds the first button that covers the specified point
    /// relative to row's position's origin
    fn find_button_by_position(&mut self, point: c::Point)
        -> Option<&mut Box<Button>>
    {
        let row_bounds = self.bounds.as_ref().expect("Missing bounds on row");
        let origin = c::Point {
            x: row_bounds.x,
            y: row_bounds.y,
        };
        let angle = self.angle;
        self.buttons.iter_mut().find(|button| {
            let bounds = button.bounds
                .as_ref().expect("Missing bounds on button")
                .clone();
            let point = point.clone();
            let origin = origin.clone();
            procedures::is_point_inside(bounds, point, origin, angle)
        })
    }
}

pub struct View {
    /// Position relative to keyboard origin
    bounds: c::Bounds,
    rows: Vec<Box<Row>>,
}

impl View {
    /// Determines the positions of rows based on their sizes
    /// Each row will be centered horizontally
    /// The collection of rows will not be altered vertically
    /// (TODO: maybe make view bounds a constraint,
    /// and derive a scaling factor that lets contents fit into view)
    /// (or TODO: blow up view bounds to match contents
    /// and then scale the entire thing)
    fn calculate_row_positions(&self, sizes: Vec<Size>) -> Vec<c::Bounds> {
        let mut y_offset = self.bounds.y;
        sizes.into_iter().map(|size| {
            let position = c::Bounds {
                x: (self.bounds.width - size.width) / 2f64,
                y: y_offset,
                width: size.width,
                height: size.height,
            };
            y_offset += size.height + ROW_SPACING;
            position
        }).collect()
    }

    /// Uses button outline information to place all buttons and rows inside.
    /// The view itself will not be affected by the sizes
    fn place_buttons_with_sizes(
        &mut self,
        button_outlines: Vec<Vec<c::Bounds>>
    ) {
        // Determine all positions
        let button_positions: Vec<_>
            = button_outlines.into_iter()
                .map(Row::calculate_button_positions)
                .collect();
        
        let row_sizes = button_positions.iter()
            .map(Row::calculate_row_size)
            .collect();

        let row_positions = self.calculate_row_positions(row_sizes);

        // Apply all positions
        for ((mut row, row_position), button_positions)
            in self.rows.iter_mut()
                .zip(row_positions)
                .zip(button_positions) {
            row.bounds = Some(row_position);
            for (mut button, button_position)
                in row.buttons.iter_mut()
                    .zip(button_positions) {
                button.bounds = Some(button_position);
            }
        }
    }

    /// Finds the first button that covers the specified point
    /// relative to view's position's origin
    fn find_button_by_position(&mut self, point: c::Point)
        -> Option<&mut Box<Button>>
    {
        // make point relative to the inside of the view,
        // which is the origin of all rows
        let point = c::Point {
            x: point.x - self.bounds.x,
            y: point.y - self.bounds.y,
        };

        self.rows.iter_mut().find_map(
            |row| row.find_button_by_position(point.clone())
        )
    }
}

mod procedures {
    use super::*;
    
    type Path<'v> = (&'v Box<Row>, &'v Box<Button>);

    /// Finds all `(row, button)` paths that refer to the specified key `state`
    pub fn find_key_paths<'v, 's>(
        view: &'v View,
        state: &'s Rc<RefCell<KeyState>>
    ) -> Vec<Path<'v>> {
        view.rows.iter().flat_map(|row| {
            let row_paths: Vec<Path> = row.buttons.iter().filter_map(|button| {
                if Rc::ptr_eq(&button.state, state) {
                    Some((row, button))
                } else {
                    None
                }
            }).collect(); // collecting not to let row references outlive the function
            row_paths.into_iter()
        }).collect()
    }
    
    /// Checks if point is inside bounds which are rotated by angle.
    /// FIXME: what's origin about?
    pub fn is_point_inside(
        bounds: c::Bounds,
        point: c::Point,
        origin: c::Point,
        angle: i32
    ) -> bool {
        (unsafe {
            c::procedures::eek_are_bounds_inside(bounds, point, origin, angle)
        }) == 1
    }
}
