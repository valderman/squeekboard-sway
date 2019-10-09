/*!
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
use std::collections::HashMap;
use std::ffi::CString;
use std::rc::Rc;
use std::vec::Vec;

use ::keyboard::*;
use ::float_ord::FloatOrd;
use ::symbol::*;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;

    use std::ffi::CStr;
    use std::os::raw::{ c_char, c_void };
    use std::ptr;

    // The following defined in C

    #[repr(transparent)]
    pub struct UserData(*const c_void);

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
    fn squeek_view_get_bounds(view: *const ::layout::View) -> Bounds {
        unsafe { &*view }.bounds.clone()
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
    fn squeek_button_get_bounds(button: *const ::layout::Button) -> Bounds {
        let button = unsafe { &*button };
        button.bounds.clone()
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

    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_label(
        button: *const ::layout::Button
    ) -> *const c_char {
        let button = unsafe { &*button };
        match &button.label {
            Label::Text(text) => text.as_ptr(),
            // returning static strings to C is a bit cumbersome
            Label::IconName(_) => unsafe {
                // CStr doesn't allocate anything, so it only points to
                // the 'static str, avoiding a memory leak
                CStr::from_bytes_with_nul_unchecked(b"icon\0")
            }.as_ptr(),
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_icon_name(button: *const Button) -> *const c_char {
        let button = unsafe { &*button };
        match &button.label {
            Label::Text(_) => ptr::null(),
            Label::IconName(name) => name.as_ptr(),
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_name(button: *const Button) -> *const c_char {
        let button = unsafe { &*button };
        button.name.as_ptr()
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_outline_name(button: *const Button) -> *const c_char {
        let button = unsafe { &*button };
        button.outline_name.as_ptr()
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
    
    #[no_mangle]
    pub extern "C"
    fn squeek_layout_get_current_view(layout: *const Layout) -> *const View {
        let layout = unsafe { &*layout };
        let view_name = layout.current_view.clone();
        layout.views.get(&view_name)
            .expect("Current view doesn't exist")
            .as_ref() as *const View
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_layout_get_keymap(layout: *const Layout) -> *const c_char {
        let layout = unsafe { &*layout };
        layout.keymap_str.as_ptr()
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_layout_free(layout: *mut Layout) {
        unsafe { Box::from_raw(layout) };
    }
    
    
    /// Entry points for more complex procedures and algoithms which span multiple modules
    pub mod procedures {
        use super::*;

        #[repr(C)]
        #[derive(PartialEq, Debug)]
        pub struct ButtonPlace {
            row: *const Row,
            button: *const Button,
        }
        
        #[repr(transparent)]
        pub struct LevelKeyboard(*const c_void);

        #[no_mangle]
        extern "C" {
            /// Checks if point falls within bounds,
            /// which are relative to origin and rotated by angle (I think)
            pub fn eek_are_bounds_inside (bounds: Bounds,
                point: Point,
                origin: Point,
                angle: i32
            ) -> u32;
            
            pub fn eek_keyboard_set_key_locked(
                keyboard: *mut LevelKeyboard,
                key: ::keyboard::c::CKeyState,
            );
        }
        
        #[no_mangle]
        pub extern "C"
        fn squeek_layout_set_state_from_press(
            layout: *mut Layout,
            keyboard: *mut LevelKeyboard,
            key: ::keyboard::c::CKeyState,
        ) {
            let layout = unsafe { &mut *layout };

            let view_name = match key.to_owned().action {
                ::symbol::Action::SetLevel(name) => {
                    Some(name.clone())
                },
                ::symbol::Action::LockLevel { lock, unlock } => {
                    let locked = {
                        let key = key.clone_ref();
                        let mut key = key.borrow_mut();
                        key.locked ^= true;
                        key.locked
                    };

                    if locked {
                        unsafe {
                            eek_keyboard_set_key_locked(
                                keyboard,
                                key
                            )
                        }
                    }

                    Some(if locked { lock } else { unlock }.clone())
                },
                _ => None,
            };

            if let Some(view_name) = view_name {
                if let Err(_e) = layout.set_view(view_name.clone()) {
                    eprintln!("No such view: {}, ignoring switch", view_name)
                };
            };
        }

        /// Places each button in order, starting from 0 on the left,
        /// keeping the spacing.
        /// Sizes each button according to outline dimensions.
        /// Places each row in order, starting from 0 on the top,
        /// keeping the spacing.
        /// Sets button and row sizes according to their contents.
        #[no_mangle]
        pub extern "C"
        fn squeek_layout_place_contents(
            layout: *mut Layout,
        ) {
            let layout = unsafe { &mut *layout };
            for view in layout.views.values_mut() {
                let sizes: Vec<Vec<Bounds>> = view.rows.iter().map(|row| {
                    row.buttons.iter()
                        .map(|button| button.bounds.clone())
                        .collect()
                }).collect();
                let spacing = view.spacing.clone();
                view.place_buttons_with_sizes(sizes, spacing);
            }
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

            use super::super::test::*;

            #[test]
            fn row_has_button() {
                let state = make_state();
                let button = make_button_with_state(
                    "test".into(),
                    state.clone()
                );
                let button_ptr = button_as_raw(&button);
                let mut row = Row::new(0);
                row.buttons.push(button);
                assert_eq!(squeek_row_contains(&row, button_ptr), true);
                let shared_button = make_button_with_state(
                    "test2".into(),
                    state
                );
                let shared_button_ptr = button_as_raw(&shared_button);
                row.buttons.push(shared_button);
                assert_eq!(squeek_row_contains(&row, shared_button_ptr), true);
                let row = Row::new(0);
                assert_eq!(squeek_row_contains(&row, button_ptr), false);
            }

            #[test]
            fn view_has_button() {
                let state = make_state();
                let state_clone = ::keyboard::c::CKeyState::wrap(state.clone());

                let button = make_button_with_state("1".into(), state);
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
                    spacing: Spacing {
                        button: 0f64,
                        row: 0f64,
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
                    spacing: Spacing {
                        button: 0f64,
                        row: 0f64,
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
        
        use ::keyboard::c::CKeyState;

        pub fn make_state() -> Rc<RefCell<::keyboard::KeyState>> {
            Rc::new(RefCell::new(::keyboard::KeyState {
                pressed: false,
                locked: false,
                keycode: None,
                action: Action::SetLevel("default".into()),
            }))
        }

        pub fn make_button_with_state(
            name: String,
            state: Rc<RefCell<::keyboard::KeyState>>,
        ) -> Box<Button> {
            Box::new(Button {
                name: CString::new(name.clone()).unwrap(),
                bounds: c::Bounds {
                    x: 0f64, y: 0f64, width: 0f64, height: 0f64
                },
                outline_name: CString::new("test").unwrap(),
                label: Label::Text(CString::new(name).unwrap()),
                state: state,
            })
        }

        pub fn button_as_raw(button: &Box<Button>) -> *const Button {
            button.as_ref() as *const Button
        }

        #[test]
        fn button_has_key() {
            let state = make_state();
            let button = make_button_with_state("1".into(), state.clone());
            assert_eq!(
                squeek_button_has_key(
                    button_as_raw(&button),
                    CKeyState::wrap(state.clone())
                ),
                1
            );
            let other_state = make_state();
            let other_button = make_button_with_state("1".into(), other_state);
            assert_eq!(
                squeek_button_has_key(
                    button_as_raw(&other_button),
                    CKeyState::wrap(state.clone())
                ),
                0
            );
            let orphan_state = CKeyState::wrap(make_state());
            assert_eq!(
                squeek_button_has_key(button_as_raw(&button), orphan_state),
                0
            );
        }
    }
}

#[derive(Debug)]
pub struct Size {
    pub width: f64,
    pub height: f64,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Label {
    /// Text used to display the symbol
    Text(CString),
    /// Icon name used to render the symbol
    IconName(CString),
}

/// The graphical representation of a button
#[derive(Clone, Debug)]
pub struct Button {
    /// ID string, e.g. for CSS 
    pub name: CString,
    /// Label to display to the user
    pub label: Label,
    /// TODO: position the buttons before they get initial bounds
    /// Position relative to some origin (i.e. parent/row)
    pub bounds: c::Bounds,
    /// The name of the visual class applied
    pub outline_name: CString,
    /// current state, shared with other buttons
    pub state: Rc<RefCell<KeyState>>,
}

/// The graphical representation of a row of buttons
pub struct Row {
    pub buttons: Vec<Box<Button>>,
    /// Angle is not really used anywhere...
    pub angle: i32,
    /// Position relative to some origin (i.e. parent/view origin)
    pub bounds: Option<c::Bounds>,
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
    
    fn calculate_button_positions(outlines: Vec<c::Bounds>, button_spacing: f64)
        -> Vec<c::Bounds>
    {
        let mut x_offset = 0f64;
        outlines.iter().map(|outline| {
            x_offset += outline.x; // account for offset outlines
            let position = c::Bounds {
                x: x_offset,
                ..outline.clone()
            };
            x_offset += outline.width + button_spacing;
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
            let bounds = button.bounds.clone();
            let point = point.clone();
            let origin = origin.clone();
            procedures::is_point_inside(bounds, point, origin, angle)
        })
    }
}

#[derive(Clone, Debug)]
pub struct Spacing {
    pub row: f64,
    pub button: f64,
}

pub struct View {
    /// Position relative to keyboard origin
    pub bounds: c::Bounds,
    pub spacing: Spacing,
    pub rows: Vec<Box<Row>>,
}

impl View {
    /// Determines the positions of rows based on their sizes
    /// Each row will be centered horizontally
    /// The collection of rows will not be altered vertically
    /// (TODO: maybe make view bounds a constraint,
    /// and derive a scaling factor that lets contents fit into view)
    /// (or TODO: blow up view bounds to match contents
    /// and then scale the entire thing)
    fn calculate_row_positions(&self, sizes: Vec<Size>, row_spacing: f64)
        -> Vec<c::Bounds>
    {
        let mut y_offset = self.bounds.y;
        sizes.into_iter().map(|size| {
            let position = c::Bounds {
                x: (self.bounds.width - size.width) / 2f64,
                y: y_offset,
                width: size.width,
                height: size.height,
            };
            y_offset += size.height + row_spacing;
            position
        }).collect()
    }

    /// Uses button outline information to place all buttons and rows inside.
    /// The view itself will not be affected by the sizes
    fn place_buttons_with_sizes(
        &mut self,
        button_outlines: Vec<Vec<c::Bounds>>,
        spacing: Spacing,
    ) {
        // Determine all positions
        let button_positions: Vec<_>
            = button_outlines.into_iter()
                .map(|outlines| {
                    Row::calculate_button_positions(outlines, spacing.button)
                })
                .collect();
        
        let row_sizes = button_positions.iter()
            .map(Row::calculate_row_size)
            .collect();

        let row_positions
            = self.calculate_row_positions(row_sizes, spacing.row);

        // Apply all positions
        for ((mut row, row_position), button_positions)
            in self.rows.iter_mut()
                .zip(row_positions)
                .zip(button_positions) {
            row.bounds = Some(row_position);
            for (mut button, button_position)
                in row.buttons.iter_mut()
                    .zip(button_positions) {
                button.bounds = button_position;
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

pub struct Layout {
    pub current_view: String,
    pub views: HashMap<String, Box<View>>,
    // TODO: move to ::keyboard::Keyboard
    pub keymap_str: CString,
}

struct NoSuchView;

impl Layout {
    fn set_view(&mut self, view: String) -> Result<(), NoSuchView> {
        if self.views.contains_key(&view) {
            self.current_view = view;
            Ok(())
        } else {
            Err(NoSuchView)
        }
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
