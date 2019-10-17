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
use std::collections::{ HashMap, HashSet };
use std::ffi::CString;
use std::rc::Rc;
use std::vec::Vec;

use ::action::Action;
use ::float_ord::FloatOrd;
use ::keyboard::{ KeyState, PressType };
use ::submission::{ Timestamp, VirtualKeyboard };

use std::borrow::Borrow;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;

    use std::ffi::CStr;
    use std::os::raw::{ c_char, c_void };
    use std::ptr;

    // The following defined in C

    #[repr(transparent)]
    pub struct UserData(*const c_void);

    #[repr(transparent)]
    #[derive(Copy, Clone)]
    pub struct EekGtkKeyboard(*const c_void);

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

        use ::submission::c::ZwpVirtualKeyboardV1;

        #[repr(C)]
        #[derive(PartialEq, Debug)]
        pub struct CButtonPlace {
            row: *const Row,
            button: *const Button,
        }

        impl<'a> From<ButtonPlace<'a>> for CButtonPlace {
            fn from(value: ButtonPlace<'a>) -> CButtonPlace {
                CButtonPlace {
                    row: value.row as *const Row,
                    button: value.button as *const Button,
                }
            }
        }
        
        /// Scale + translate
        #[repr(C)]
        pub struct Transformation {
            origin_x: f64,
            origin_y: f64,
            scale: f64,
        }
        
        impl Transformation {
            fn forward(&self, p: Point) -> Point {
                Point {
                    x: (p.x - self.origin_x) / self.scale,
                    y: (p.y - self.origin_y) / self.scale,
                }
            }
        }

        // This is constructed only in C, no need for warnings
        #[allow(dead_code)]
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

            // Button and View are safe to pass to C
            // as long as they don't outlive the call
            // and nothing dereferences them
            #[allow(improper_ctypes)]
            pub fn eek_gtk_on_button_released(
                button: *const Button,
                view: *const View,
                keyboard: EekGtkKeyboard,
            );

            // Button and View inside CButtonPlace are safe to pass to C
            // as long as they don't outlive the call
            // and nothing dereferences them
            #[allow(improper_ctypes)]
            pub fn eek_gtk_on_button_pressed(
                place: CButtonPlace,
                keyboard: EekGtkKeyboard,
            );
            
            // Button and View inside CButtonPlace are safe to pass to C
            // as long as they don't outlive the call
            // and nothing dereferences them
            #[allow(improper_ctypes)]
            pub fn eek_gtk_render_locked_button(
                keyboard: EekGtkKeyboard,
                place: CButtonPlace,
            );
        }

        /// Places each button in order, starting from 0 on the left,
        /// keeping the spacing.
        /// Sizes each button according to outline dimensions.
        /// Places each row in order, starting from 0 on the top,
        /// keeping the spacing.
        /// Sets button and row sizes according to their contents.
        #[no_mangle]
        pub extern "C"
        fn squeek_layout_place_contents(layout: *mut Layout) {
            let layout = unsafe { &mut *layout };
            for view in layout.views.values_mut() {
                let sizes: Vec<Vec<Bounds>> = view.rows.iter().map(|row| {
                    row.buttons.iter()
                        .map(|button| button.bounds.clone())
                        .collect()
                }).collect();
                view.place_buttons_with_sizes(sizes);
            }
        }

        #[no_mangle]
        pub extern "C"
        fn squeek_layout_release(
            layout: *mut Layout,
            virtual_keyboard: ZwpVirtualKeyboardV1, // TODO: receive a reference to the backend
            time: u32,
            ui_keyboard: EekGtkKeyboard,
        ) {
            let layout = unsafe { &mut *layout };
            let virtual_keyboard = VirtualKeyboard(virtual_keyboard);
            // The list must be copied,
            // because it will be mutated in the loop
            for key in layout.pressed_keys.clone() {
                let key: &Rc<RefCell<KeyState>> = key.borrow();
                layout.release_key(
                    &virtual_keyboard,
                    &mut key.clone(),
                    Timestamp(time)
                );
                let view = layout.get_current_view();
                ::layout::procedures::release_ui_buttons(
                    &view, key, ui_keyboard,
                );
            }
        }

        /// Release all buittons but don't redraw
        #[no_mangle]
        pub extern "C"
        fn squeek_layout_release_all_only(
            layout: *mut Layout,
            virtual_keyboard: ZwpVirtualKeyboardV1, // TODO: receive a reference to the backend
            time: u32,
        ) {
            let layout = unsafe { &mut *layout };
            let virtual_keyboard = VirtualKeyboard(virtual_keyboard);
            // The list must be copied,
            // because it will be mutated in the loop
            for key in layout.pressed_keys.clone() {
                let key: &Rc<RefCell<KeyState>> = key.borrow();
                layout.release_key(
                    &virtual_keyboard,
                    &mut key.clone(),
                    Timestamp(time)
                );
            }
        }

        #[no_mangle]
        pub extern "C"
        fn squeek_layout_depress(
            layout: *mut Layout,
            virtual_keyboard: ZwpVirtualKeyboardV1, // TODO: receive a reference to the backend
            x_widget: f64, y_widget: f64,
            widget_to_layout: Transformation,
            time: u32,
            ui_keyboard: EekGtkKeyboard,
        ) {
            let layout = unsafe { &mut *layout };
            let view = layout.get_current_view();
            let point = widget_to_layout.forward(
                Point { x: x_widget, y: y_widget }
            );
            
            let place = view.find_button_by_position(point);
            // the immutable reference to `layout` through `view`
            // must be dropped
            // before `layout.press_key` borrows it mutably again
            let state_place = place.map(|place| {(
                place.button.state.clone(),
                place.into(),
            )});
            
            if let Some((mut state, c_place)) = state_place {
                layout.press_key(
                    &VirtualKeyboard(virtual_keyboard),
                    &mut state,
                    Timestamp(time),
                );

                unsafe { eek_gtk_on_button_pressed(c_place, ui_keyboard) };
            }
        }

        // FIXME: this will work funny
        // when 2 touch points are on buttons and moving one after another
        // Solution is to have separate pressed lists for each point
        #[no_mangle]
        pub extern "C"
        fn squeek_layout_drag(
            layout: *mut Layout,
            virtual_keyboard: ZwpVirtualKeyboardV1, // TODO: receive a reference to the backend
            x_widget: f64, y_widget: f64,
            widget_to_layout: Transformation,
            time: u32,
            ui_keyboard: EekGtkKeyboard,
        ) {
            let layout = unsafe { &mut *layout };
            let virtual_keyboard = VirtualKeyboard(virtual_keyboard);

            let view = layout.get_current_view();
            
            let point = widget_to_layout.forward(
                Point { x: x_widget, y: y_widget }
            );
            
            let pressed = layout.pressed_keys.clone();
            let place = view.find_button_by_position(point);

            let state_place = place.map(|place| {(
                place.button.state.clone(),
                place.into(),
            )});

            if let Some((mut state, c_place)) = state_place {
                let mut found = false;
                for wrapped_key in pressed {
                    let key: &Rc<RefCell<KeyState>> = wrapped_key.borrow();
                    if Rc::ptr_eq(&state, &wrapped_key.0) {
                        found = true;
                    } else {
                        layout.release_key(
                            &virtual_keyboard,
                            &mut key.clone(), 
                            Timestamp(time),
                        );
                        let view = layout.get_current_view();
                        ::layout::procedures::release_ui_buttons(
                            &view, key, ui_keyboard,
                        );
                    }
                }
                if !found {
                    layout.press_key(
                        &virtual_keyboard,
                        &mut state,
                        Timestamp(time),
                    );
                    unsafe { eek_gtk_on_button_pressed(c_place, ui_keyboard) };
                }
            } else {
                for wrapped_key in pressed {
                    let key: &Rc<RefCell<KeyState>> = wrapped_key.borrow();
                    layout.release_key(
                        &virtual_keyboard,
                        &mut key.clone(), 
                        Timestamp(time),
                    );
                    let view = layout.get_current_view();
                    ::layout::procedures::release_ui_buttons(
                        &view, key, ui_keyboard,
                    );
                }
            }
        }

        #[no_mangle]
        pub extern "C"
        fn squeek_layout_draw_all_changed(
            layout: *mut Layout,
            ui_keyboard: EekGtkKeyboard,
        ) {
            let layout = unsafe { &mut *layout };
            
            for row in &layout.get_current_view().rows {
                for button in &row.buttons {
                    let c_place = CButtonPlace::from(
                        ButtonPlace { row, button }
                    );
                    let state = RefCell::borrow(&button.state);
                    match (state.pressed, state.locked) {
                        (PressType::Released, false) => {}
                        (PressType::Pressed, _) => unsafe {
                            eek_gtk_on_button_pressed(c_place, ui_keyboard)
                        },
                        (_, true) => unsafe {
                            eek_gtk_render_locked_button(ui_keyboard, c_place)
                        },
                    }
                }
            }
        }
    }
}

pub struct ButtonPlace<'a> {
    button: &'a Button,
    row: &'a Row,
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
    fn last(positions: &Vec<c::Bounds>) -> Option<&c::Bounds> {
        let len = positions.len();
        match len {
            0 => None,
            l => Some(&positions[l - 1])
        }
    }
    
    fn calculate_button_positions(outlines: Vec<c::Bounds>) -> Vec<c::Bounds> {
        let mut x_offset = 0f64;
        outlines.iter().map(|outline| {
            x_offset += outline.x; // account for offset outlines
            let position = c::Bounds {
                x: x_offset,
                ..outline.clone()
            };
            x_offset += outline.width;
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
    fn find_button_by_position(&self, point: c::Point)
        -> Option<&Box<Button>>
    {
        let row_bounds = self.bounds.as_ref().expect("Missing bounds on row");
        let origin = c::Point {
            x: row_bounds.x,
            y: row_bounds.y,
        };
        let angle = self.angle;
        self.buttons.iter().find(|button| {
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
    fn calculate_row_positions(&self, sizes: Vec<Size>) -> Vec<c::Bounds> {
        let mut y_offset = self.bounds.y;
        sizes.into_iter().map(|size| {
            let position = c::Bounds {
                x: (self.bounds.width - size.width) / 2f64,
                y: y_offset,
                width: size.width,
                height: size.height,
            };
            y_offset += size.height;
            position
        }).collect()
    }

    /// Uses button outline information to place all buttons and rows inside.
    /// The view itself will not be affected by the sizes
    fn place_buttons_with_sizes(
        &mut self,
        button_outlines: Vec<Vec<c::Bounds>>,
    ) {
        // Determine all positions
        let button_positions: Vec<_>
            = button_outlines.into_iter()
                .map(|outlines| {
                    Row::calculate_button_positions(outlines)
                })
                .collect();
        
        let row_sizes = button_positions.iter()
            .map(Row::calculate_row_size)
            .collect();

        let row_positions
            = self.calculate_row_positions(row_sizes);

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
    fn find_button_by_position(&self, point: c::Point)
        -> Option<ButtonPlace>
    {
        // make point relative to the inside of the view,
        // which is the origin of all rows
        let point = c::Point {
            x: point.x - self.bounds.x,
            y: point.y - self.bounds.y,
        };

        self.rows.iter().find_map(|row| {
            row.find_button_by_position(point.clone())
                .map(|button| ButtonPlace {row, button})
        })
    }
}

// TODO: split into sth like
// Arrangement (views) + details (keymap) + State (keys)
/// State of the UI, contains the backend as well
pub struct Layout {
    pub current_view: String,
    // Views own the actual buttons which have state
    // Maybe they should own UI only,
    // and keys should be owned by a dedicated non-UI-State?
    pub views: HashMap<String, Box<View>>,

    // Non-UI stuff
    /// xkb keymap applicable to the contained keys. Unchangeable
    pub keymap_str: CString,
    // Changeable state
    // a Vec would be enough, but who cares, this will be small & fast enough
    pub pressed_keys: HashSet<::util::Pointer<RefCell<KeyState>>>,
    pub locked_keys: HashSet<::util::Pointer<RefCell<KeyState>>>,
}

struct NoSuchView;

// Unfortunately, changes are not atomic due to mutability :(
// An error will not be recoverable
// The usage of &mut on Rc<RefCell<KeyState>> doesn't mean anything special.
// Cloning could also be used.
impl Layout {
    fn get_current_view(&self) -> &Box<View> {
        self.views.get(&self.current_view).expect("Selected nonexistent view")
    }

    fn set_view(&mut self, view: String) -> Result<(), NoSuchView> {
        if self.views.contains_key(&view) {
            self.current_view = view;
            Ok(())
        } else {
            Err(NoSuchView)
        }
    }

    fn release_key(
        &mut self,
        virtual_keyboard: &VirtualKeyboard,
        mut key: &mut Rc<RefCell<KeyState>>,
        time: Timestamp,
    ) {
        if !self.pressed_keys.remove(&::util::Pointer(key.clone())) {
            eprintln!("Warning: key {:?} was not pressed", key);
        }
        virtual_keyboard.switch(
            &mut key.borrow_mut(),
            PressType::Released,
            time,
        );
        self.set_level_from_press(&mut key);
    }
    
    fn press_key(
        &mut self,
        virtual_keyboard: &VirtualKeyboard,
        key: &mut Rc<RefCell<KeyState>>,
        time: Timestamp,
    ) {
        if !self.pressed_keys.insert(::util::Pointer(key.clone())) {
            eprintln!("Warning: key {:?} was already pressed", key);
        }
        virtual_keyboard.switch(
            &mut key.borrow_mut(),
            PressType::Pressed,
            time,
        );
    }

    fn set_level_from_press(&mut self, key: &Rc<RefCell<KeyState>>) {
        let keys = self.locked_keys.clone();
        for key in &keys {
            self.locked_keys.remove(key);
            self.set_state_from_press(key.borrow());
        }

        // Don't handle the same key twice, but handle it at least once,
        // because its press is the reason we're here
        if !keys.contains(&::util::Pointer(key.clone())) {
            self.set_state_from_press(key);
        }
    }

    fn set_state_from_press(&mut self, key: &Rc<RefCell<KeyState>>) {
        // Action should not hold a reference to key,
        // because key is later borrowed for mutation. So action is cloned.
        // RefCell::borrow() is covered up by (dyn Borrow)::borrow()
        // if used like key.borrow() :(
        let action = RefCell::borrow(key).action.clone();
        let view_name = match action {
            Action::SetLevel(name) => {
                Some(name.clone())
            },
            Action::LockLevel { lock, unlock } => {
                let locked = {
                    let mut key = key.borrow_mut();
                    key.locked ^= true;
                    key.locked
                };

                if locked {
                    self.locked_keys.insert(::util::Pointer(key.clone()));
                }

                Some(if locked { lock } else { unlock }.clone())
            },
            _ => None,
        };

        if let Some(view_name) = view_name {
            if let Err(_e) = self.set_view(view_name.clone()) {
                eprintln!("No such view: {}, ignoring switch", view_name)
            };
        };
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

    /// Switch off all UI buttons associated with the (state) key
    pub fn release_ui_buttons(
        view: &Box<View>,
        key: &Rc<RefCell<KeyState>>,
        ui_keyboard: c::EekGtkKeyboard,
    ) {
        let paths = ::layout::procedures::find_key_paths(&view, key);
        for (_row, button) in paths {
            unsafe {
                c::procedures::eek_gtk_on_button_released(
                    button.as_ref() as *const Button,
                    view.as_ref() as *const View,
                    ui_keyboard,
                );
            };
        }
    }
    
    #[cfg(test)]
    mod test {
        use super::*;

        use ::layout::test::*;

        /// Checks whether the path points to the same boxed instances.
        /// The instance constraint will be droppable
        /// when C stops holding references to the data
        #[test]
        fn view_has_button() {
            fn as_ptr<T>(v: &Box<T>) -> *const T {
                v.as_ref() as *const T
            }

            let state = make_state();
            let state_clone = state.clone();

            let button = make_button_with_state("1".into(), state);
            let button_ptr = as_ptr(&button);
            
            let row = Box::new(Row {
                buttons: vec!(button),
                angle: 0,
                bounds: None
            });
            let row_ptr = as_ptr(&row);

            let view = View {
                bounds: c::Bounds {
                    x: 0f64, y: 0f64,
                    width: 0f64, height: 0f64
                },
                rows: vec!(row),
            };

            assert_eq!(
                find_key_paths(&view, &state_clone.clone()).iter()
                    .map(|(row, button)| { (as_ptr(row), as_ptr(button)) })
                    .collect::<Vec<_>>(),
                vec!(
                    (row_ptr, button_ptr)
                )
            );

            let view = View {
                bounds: c::Bounds {
                    x: 0f64, y: 0f64,
                    width: 0f64, height: 0f64
                },
                rows: Vec::new(),
            };
            assert_eq!(
                find_key_paths(&view, &state_clone.clone()).is_empty(),
                true
            );
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use std::ffi::CString;

    pub fn make_state() -> Rc<RefCell<::keyboard::KeyState>> {
        Rc::new(RefCell::new(::keyboard::KeyState {
            pressed: PressType::Released,
            locked: false,
            keycodes: Vec::new(),
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
}
