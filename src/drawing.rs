/*! Drawing the UI */

use cairo;
use std::cell::RefCell;

use ::keyboard;
use ::layout;
use ::layout::{ Button, Layout };
use ::layout::c::{ EekGtkKeyboard, Point };

use gdk::{ WindowExt, DrawingContextExt };
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
        
        pub fn eek_gtk_keyboard_get_renderer(
            keyboard: EekGtkKeyboard,
        ) -> EekRenderer;
        
        pub fn eek_renderer_get_transformation(
            renderer: EekRenderer,
        ) -> layout::c::Transformation;
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

/// Renders a single frame
/// Opens a frame on `keyboard`'s `GdkWindow`, attempt to get a drawing context,
/// calls `f`, closes the frame.
/// If the drawing context was successfully retrieved, returns `f` call result.
pub fn render_as_frame<F, T>(keyboard: EekGtkKeyboard, mut f: F) -> Option<T>
    where F: FnMut(c::EekRenderer, &cairo::Context) -> T
{
    let renderer = unsafe { c::eek_gtk_keyboard_get_renderer(keyboard) };
    
    let widget = unsafe { gtk::Widget::from_glib_none(keyboard.0) };
    widget.get_window()
        .and_then(|window| {
            // Need to split the `.and_then` chain here
            // because `window` needs to be in scope
            // for the references deeper inside.
            window.get_clip_region()
                // contrary to the docs, `Region` gets destroyed automatically
                .and_then(|region| window.begin_draw_frame(&region))
                .and_then(|drawctx| {
                    let ret: Option<T> = drawctx.get_cairo_context()
                        .map(|cr| {
                            let transformation = unsafe {
                                c::eek_renderer_get_transformation(renderer)
                            };
                            cr.translate(
                                transformation.origin_x,
                                transformation.origin_y,
                            );
                            cr.scale(
                                transformation.scale,
                                transformation.scale,
                            );
                            queue_redraw(keyboard);
                            f(renderer, &cr) // finally!
                        });
                    // This must always happen after `begin_draw_frame`,
                    // enven if `get_cairo_context` fails.
                    window.end_draw_frame(&drawctx);
                    ret
                })
        })
}
