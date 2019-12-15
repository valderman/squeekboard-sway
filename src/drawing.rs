/*! Drawing the UI */

use cairo;
use std::cell::RefCell;

use ::keyboard;
use ::layout::{ Button, Layout };
use ::layout::c::{ EekGtkKeyboard, Point };

use glib::translate::FromGlibPtrNone;
use gtk::WidgetExt;

mod c {
    use super::*;

    use cairo_sys;
    use std::os::raw::c_void;
    
    // This is constructed only in C, no need for warnings
    #[allow(dead_code)]
    #[repr(transparent)]
    #[derive(Clone, Copy)]
    pub struct EekRenderer(*const c_void);

    #[no_mangle]
    extern "C" {
        // Button and View inside CButtonPlace are safe to pass to C
        // as long as they don't outlive the call
        // and nothing dereferences them
        #[allow(improper_ctypes)]
        pub fn eek_render_button(
            renderer: EekRenderer,
            cr: *mut cairo_sys::cairo_t,
            button: *const Button,
            pressed: u64,
            locked: u64,
        );
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_layout_draw_all_changed(
        layout: *mut Layout,
        renderer: EekRenderer,
        cr: *mut cairo_sys::cairo_t,
    ) {
        let layout = unsafe { &mut *layout };
        let cr = unsafe { cairo::Context::from_raw_none(cr) };

        let view = layout.get_current_view();
        let view_position = view.bounds.get_position();
        for row in &view.rows {
            for button in &row.buttons {
                let state = RefCell::borrow(&button.state).clone();
                if state.pressed == keyboard::PressType::Pressed || state.locked {
                    let position = &view_position
                        + row.bounds.clone().unwrap().get_position()
                        + button.bounds.get_position();
                    render_button_at_position(
                        renderer, &cr,
                        position, button.as_ref(),
                        state.pressed, state.locked,
                    );
                }
            }
        }
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_draw_layout_base_view(
        layout: *mut Layout,
        renderer: EekRenderer,
        cr: *mut cairo_sys::cairo_t,
    ) {
        let layout = unsafe { &mut *layout };
        let cr = unsafe { cairo::Context::from_raw_none(cr) };
        let view = layout.get_current_view();
        let view_position = view.bounds.get_position();
        for row in &view.rows {
            for button in &row.buttons {
                let position = &view_position
                    + row.bounds.clone().unwrap().get_position()
                    + button.bounds.get_position();
                render_button_at_position(
                    renderer, &cr,
                    position, button.as_ref(),
                    keyboard::PressType::Released, false,
                );
            }
        }
    }
}

/// Renders a button at a position (button's own bounds ignored)
pub fn render_button_at_position(
    renderer: c::EekRenderer,
    cr: &cairo::Context,
    position: Point,
    button: &Button,
    pressed: keyboard::PressType,
    locked: bool,
) {
    cr.save();
    cr.translate(position.x, position.y);
    cr.rectangle(
        0.0, 0.0,
        button.bounds.width, button.bounds.height
    );
    cr.clip();
    unsafe {
        c::eek_render_button(
            renderer,
            cairo::Context::to_raw_none(&cr),
            button as *const Button,
            pressed as u64,
            locked as u64,
        )
    };
    cr.restore();
}

pub fn queue_redraw(keyboard: EekGtkKeyboard) {
    let widget = unsafe { gtk::Widget::from_glib_none(keyboard.0) };
    widget.queue_draw();
}
