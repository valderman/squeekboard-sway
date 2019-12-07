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
use ::drawing;
use ::keyboard::{ KeyState, PressType };
use ::submission::{ Timestamp, VirtualKeyboard };
use ::util::find_max_double;

use std::borrow::Borrow;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;

    use gtk_sys;
    use std::ffi::CStr;
    use std::os::raw::{ c_char, c_void };
    use std::ptr;

    use std::ops::{ Add, Sub };

    // The following defined in C
    #[repr(transparent)]
    #[derive(Copy, Clone)]
    pub struct EekGtkKeyboard(pub *const gtk_sys::GtkWidget);

    /// Defined in eek-types.h
    #[repr(C)]
    #[derive(Clone, Debug, PartialEq)]
    pub struct Point {
        pub x: f64,
        pub y: f64,
    }
    
    impl Add for Point {
        type Output = Self;
        fn add(self, other: Self) -> Self {
            &self + other
        }
    }

    impl Add<Point> for &Point {
        type Output = Point;
        fn add(self, other: Point) -> Point {
            Point {
                x: self.x + other.x,
                y: self.y + other.y,
            }
        }
    }
    
    impl Sub<&Point> for Point {
        type Output = Point;
        fn sub(self, other: &Point) -> Point {
            Point {
                x: self.x - other.x,
                y: self.y - other.y,
            }
        }
    }

    /// Defined in eek-types.h
    #[repr(C)]
    #[derive(Clone, Debug, PartialEq)]
    pub struct Bounds {
        pub x: f64,
        pub y: f64,
        pub width: f64,
        pub height: f64
    }

    impl Bounds {
        pub fn contains(&self, point: &Point) -> bool {
            (point.x > self.x && point.x < self.x + self.width
                && point.y > self.y && point.y < self.y + self.height)
        }
    }

    /// Scale + translate
    #[repr(C)]
    pub struct Transformation {
        pub origin_x: f64,
        pub origin_y: f64,
        pub scale: f64,
    }

    impl Transformation {
        fn forward(&self, p: Point) -> Point {
            Point {
                x: (p.x - self.origin_x) / self.scale,
                y: (p.y - self.origin_y) / self.scale,
            }
        }
        fn reverse(&self, p: Point) -> Point {
            Point {
                x: p.x * self.scale + self.origin_x,
                y: p.y * self.scale + self.origin_y,
            }
        }
        pub fn reverse_bounds(&self, b: Bounds) -> Bounds {
            let start = self.reverse(Point { x: b.x, y: b.y });
            let end = self.reverse(Point {
                x: b.x + b.width,
                y: b.y + b.height,
            });
            Bounds {
                x: start.x,
                y: start.y,
                width: end.x - start.x,
                height: end.y - start.y,
            }
        }
    }

    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers

    #[no_mangle]
    pub extern "C"
    fn squeek_button_get_bounds(button: *const ::layout::Button) -> Bounds {
        let button = unsafe { &*button };
        Bounds {
            x: 0.0, y: 0.0,
            width: button.size.width, height: button.size.height
        }
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
    
    /// Positions the layout within the available space
    #[no_mangle]
    pub extern "C"
    fn squeek_layout_calculate_transformation(
        layout: *const Layout,
        allocation_width: f64,
        allocation_height: f64,
    ) -> Transformation {
        let layout = unsafe { &*layout };
        let size = layout.calculate_size();
        let h_scale = allocation_width / size.width;
        let v_scale = allocation_height / size.height;
        let scale = if h_scale < v_scale { h_scale } else { v_scale };
        Transformation {
            origin_x: (allocation_width - (scale * size.width)) / 2.0,
            origin_y: (allocation_height - (scale * size.height)) / 2.0,
            scale: scale,
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_layout_get_keymap(layout: *const Layout) -> *const c_char {
        let layout = unsafe { &*layout };
        layout.keymap_str.as_ptr()
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_layout_get_kind(layout: *const Layout) -> u32 {
        let layout = unsafe { &*layout };
        layout.kind.clone() as u32
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

        // This is constructed only in C, no need for warnings
        #[allow(dead_code)]
        #[repr(transparent)]
        pub struct LevelKeyboard(*const c_void);

        /// Release pointer in the specified position
        #[no_mangle]
        pub extern "C"
        fn squeek_layout_release(
            layout: *mut Layout,
            virtual_keyboard: ZwpVirtualKeyboardV1, // TODO: receive a reference to the backend
            widget_to_layout: Transformation,
            time: u32,
            ui_keyboard: EekGtkKeyboard,
        ) {
            let time = Timestamp(time);
            let layout = unsafe { &mut *layout };
            let virtual_keyboard = VirtualKeyboard(virtual_keyboard);
            // The list must be copied,
            // because it will be mutated in the loop
            for key in layout.pressed_keys.clone() {
                let key: &Rc<RefCell<KeyState>> = key.borrow();
                seat::handle_release_key(
                    layout,
                    &virtual_keyboard,
                    &widget_to_layout,
                    time,
                    ui_keyboard,
                    key,
                );
            }
            drawing::queue_redraw(ui_keyboard);
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
            let point = widget_to_layout.forward(
                Point { x: x_widget, y: y_widget }
            );

            let state = {
                let view = layout.get_current_view();
                view.find_button_by_position(point)
                    .map(|place| place.button.state.clone())
            };
            
            if let Some(mut state) = state {
                layout.press_key(
                    &VirtualKeyboard(virtual_keyboard),
                    &mut state,
                    Timestamp(time),
                );
                // maybe TODO: draw on the display buffer here
                drawing::queue_redraw(ui_keyboard);
            };
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
            let time = Timestamp(time);
            let layout = unsafe { &mut *layout };
            let virtual_keyboard = VirtualKeyboard(virtual_keyboard);

            let point = widget_to_layout.forward(
                Point { x: x_widget, y: y_widget }
            );
            
            let pressed = layout.pressed_keys.clone();
            let button_info = {
                let view = layout.get_current_view();
                let place = view.find_button_by_position(point);
                place.map(|place| {(
                    place.button.state.clone(),
                    place.button.clone(),
                    place.offset,
                )})
            };

            if let Some((mut state, _button, _view_position)) = button_info {
                let mut found = false;
                for wrapped_key in pressed {
                    let key: &Rc<RefCell<KeyState>> = wrapped_key.borrow();
                    if Rc::ptr_eq(&state, &wrapped_key.0) {
                        found = true;
                    } else {
                        seat::handle_release_key(
                            layout,
                            &virtual_keyboard,
                            &widget_to_layout,
                            time,
                            ui_keyboard,
                            key,
                        );
                    }
                }
                if !found {
                    layout.press_key(&virtual_keyboard, &mut state, time);
                    // maybe TODO: draw on the display buffer here
                }
            } else {
                for wrapped_key in pressed {
                    let key: &Rc<RefCell<KeyState>> = wrapped_key.borrow();
                    seat::handle_release_key(
                        layout,
                        &virtual_keyboard,
                        &widget_to_layout,
                        time,
                        ui_keyboard,
                        key,
                    );
                }
            }
            drawing::queue_redraw(ui_keyboard);
        }

        #[cfg(test)]
        mod test {
            use super::*;
            
            fn near(a: f64, b: f64) -> bool {
                (a - b).abs() < ((a + b) * 0.001f64).abs()
            }
            
            #[test]
            fn transform_back() {
                let transform = Transformation {
                    origin_x: 10f64,
                    origin_y: 11f64,
                    scale: 12f64,
                };
                let point = Point { x: 1f64, y: 1f64 };
                let transformed = transform.reverse(transform.forward(point.clone()));
                assert!(near(point.x, transformed.x));
                assert!(near(point.y, transformed.y));
            }
        }
    }
}

pub struct ButtonPlace<'a> {
    button: &'a Button,
    offset: c::Point,
}

#[derive(Debug, Clone)]
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
    pub size: Size,
    /// The name of the visual class applied
    pub outline_name: CString,
    /// current state, shared with other buttons
    pub state: Rc<RefCell<KeyState>>,
}

/// The graphical representation of a row of buttons
pub struct Row {
    /// Buttons together with their offset from the left
    pub buttons: Vec<(f64, Box<Button>)>,
    /// Angle is not really used anywhere...
    pub angle: i32,
}

impl Row {    
    pub fn get_height(&self) -> f64 {
        find_max_double(
            self.buttons.iter(),
            |(_offset, button)| button.size.height,
        )
    }

    fn get_width(&self) -> f64 {
        self.buttons.iter().next_back()
            .map(|(x_offset, button)| button.size.width + x_offset)
            .unwrap_or(0.0)
    }

    /// Finds the first button that covers the specified point
    /// relative to row's position's origin
    fn find_button_by_position(&self, point: c::Point)
        -> Option<(f64, &Box<Button>)>
    {
        self.buttons.iter().find(|(x_offset, button)| {
            c::Bounds {
                x: *x_offset, y: 0.0,
                width: button.size.width,
                height: button.size.height,
            }.contains(&point)
        })
            .map(|(x_offset, button)| (*x_offset, button))
    }
}

#[derive(Clone, Debug)]
pub struct Spacing {
    pub row: f64,
    pub button: f64,
}

pub struct View {
    /// Rows together with their offsets from the top
    rows: Vec<(f64, Row)>,
}

impl View {
    pub fn new(rows: Vec<(f64, Row)>) -> View {
        View { rows }
    }
    /// Finds the first button that covers the specified point
    /// relative to view's position's origin
    fn find_button_by_position(&self, point: c::Point)
        -> Option<ButtonPlace>
    {
        self.get_rows().iter().find_map(|(row_offset, row)| {
            // make point relative to the inside of the row
            row.find_button_by_position({
                c::Point { x: point.x, y: point.y } - row_offset
            }).map(|(button_x_offset, button)| ButtonPlace {
                button,
                offset: row_offset + c::Point {
                    x: button_x_offset,
                    y: 0.0,
                },
            })
        })
    }
    
    fn get_width(&self) -> f64 {
        // No need to call `get_rows()`,
        // as the biggest row is the most far reaching in both directions
        // because they are all centered.
        find_max_double(self.rows.iter(), |(_offset, row)| row.get_width())
    }
    
    fn get_height(&self) -> f64 {
        self.rows.iter().next_back()
            .map(|(y_offset, row)| row.get_height() + y_offset)
            .unwrap_or(0.0)
    }
    
    /// Returns positioned rows, with appropriate x offsets (centered)
    pub fn get_rows(&self) -> Vec<(c::Point, &Row)> {
        let available_width = self.get_width();
        self.rows.iter().map(|(y_offset, row)| {(
            c::Point {
                x: (available_width - row.get_width()) / 2.0,
                y: *y_offset,
            },
            row,
        )}).collect()
    }
}

/// The physical characteristic of layout for the purpose of styling
#[derive(Clone, PartialEq, Debug)]
pub enum ArrangementKind {
    Base = 0,
    Wide = 1,
}

pub struct Margins {
    pub top: f64,
    pub bottom: f64,
    pub left: f64,
    pub right: f64,
}

// TODO: split into sth like
// Arrangement (views) + details (keymap) + State (keys)
/// State of the UI, contains the backend as well
pub struct Layout {
    pub margins: Margins,
    pub kind: ArrangementKind,
    pub current_view: String,
    // Views own the actual buttons which have state
    // Maybe they should own UI only,
    // and keys should be owned by a dedicated non-UI-State?
    pub views: HashMap<String, View>,

    // Non-UI stuff
    /// xkb keymap applicable to the contained keys. Unchangeable
    pub keymap_str: CString,
    // Changeable state
    // a Vec would be enough, but who cares, this will be small & fast enough
    // TODO: turn those into per-input point *_buttons to track dragging.
    // The renderer doesn't need the list of pressed keys any more,
    // because it needs to iterate
    // through all buttons of the current view anyway.
    // When the list tracks actual location,
    // it becomes possible to place popovers and other UI accurately.
    pub pressed_keys: HashSet<::util::Pointer<RefCell<KeyState>>>,
    pub locked_keys: HashSet<::util::Pointer<RefCell<KeyState>>>,
}

/// A builder structure for picking up layout data from storage
pub struct LayoutData {
    pub views: HashMap<String, View>,
    pub keymap_str: CString,
    pub margins: Margins,
}

struct NoSuchView;

// Unfortunately, changes are not atomic due to mutability :(
// An error will not be recoverable
// The usage of &mut on Rc<RefCell<KeyState>> doesn't mean anything special.
// Cloning could also be used.
impl Layout {
    pub fn new(data: LayoutData, kind: ArrangementKind) -> Layout {
        Layout {
            kind,
            current_view: "base".to_owned(),
            views: data.views,
            keymap_str: data.keymap_str,
            pressed_keys: HashSet::new(),
            locked_keys: HashSet::new(),
            margins: data.margins,
        }
    }

    pub fn get_current_view(&self) -> &View {
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

    fn calculate_size(&self) -> Size {
        Size {
            height: find_max_double(
                self.views.iter(),
                |(_name, view)| view.get_height(),
            ),
            width: find_max_double(
                self.views.iter(),
                |(_name, view)| view.get_width(),
            ),
        }
    }
}

mod procedures {
    use super::*;

    type Place<'v> = (c::Point, &'v Box<Button>);

    /// Finds all buttons referring to the key in `state`,
    /// together with their offsets within the view.
    pub fn find_key_places<'v, 's>(
        view: &'v View,
        state: &'s Rc<RefCell<KeyState>>
    ) -> Vec<Place<'v>> {
        view.get_rows().iter().flat_map(|(row_offset, row)| {
            row.buttons.iter()
                .filter_map(move |(x_offset, button)| {
                    if Rc::ptr_eq(&button.state, state) {
                        Some((
                            row_offset + c::Point { x: *x_offset, y: 0.0 },
                            button,
                        ))
                    } else {
                        None
                    }
                })
        }).collect()
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

            let row = Row {
                buttons: vec!((0.1, button)),
                angle: 0,
            };

            let view = View {
                rows: vec!((1.2, row)),
            };

            assert_eq!(
                find_key_places(&view, &state_clone.clone()).into_iter()
                    .map(|(place, button)| { (place, as_ptr(button)) })
                    .collect::<Vec<_>>(),
                vec!(
                    (c::Point { x: 0.1, y: 1.2 }, button_ptr)
                )
            );

            let view = View {
                rows: Vec::new(),
            };
            assert_eq!(
                find_key_places(&view, &state_clone.clone()).is_empty(),
                true
            );
        }
    }
}

/// Top level procedures, dispatching to everything
mod seat {
    use super::*;

    // TODO: turn into release_button
    pub fn handle_release_key(
        layout: &mut Layout,
        virtual_keyboard: &VirtualKeyboard,
        widget_to_layout: &c::Transformation,
        time: Timestamp,
        ui_keyboard: c::EekGtkKeyboard,
        key: &Rc<RefCell<KeyState>>,
    ) {
        layout.release_key(virtual_keyboard, &mut key.clone(), time);

        let view = layout.get_current_view();
        let action = RefCell::borrow(key).action.clone();
        if let Action::ShowPreferences = action {
            let places = ::layout::procedures::find_key_places(
                view, key
            );
            // getting first item will cause mispositioning
            // with more than one button with the same key
            // on the keyboard
            if let Some((offset, button)) = places.get(0) {
                let bounds = c::Bounds {
                    x: offset.x, y: offset.y,
                    width: button.size.width,
                    height: button.size.height,
                };
                ::popover::show(
                    ui_keyboard,
                    widget_to_layout.reverse_bounds(bounds)
                );
            }
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
            size: Size { width: 0f64, height: 0f64 },
            outline_name: CString::new("test").unwrap(),
            label: Label::Text(CString::new(name).unwrap()),
            state: state,
        })
    }
    
    #[test]
    fn check_centering() {
        //    foo
        // ---bar---
        let view = View::new(vec![
            (
                0.0,
                Row {
                    angle: 0,
                    buttons: vec![(
                        0.0,
                        Box::new(Button {
                            size: Size { width: 10.0, height: 10.0 },
                            ..*make_button_with_state("foo".into(), make_state())
                        }),
                    )]
                },
            ),
            (
                10.0,
                Row {
                    angle: 0,
                    buttons: vec![(
                        0.0,
                        Box::new(Button {
                            size: Size { width: 30.0, height: 10.0 },
                            ..*make_button_with_state("bar".into(), make_state())
                        }),
                    )]
                },
            )
        ]);
        assert!(
            view.find_button_by_position(c::Point { x: 5.0, y: 5.0 })
                .is_none()
        );
    }
}
