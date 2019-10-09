extern crate rs;
extern crate xkbcommon;

use std::env;

use rs::data::{ Layout, LoadError };

use xkbcommon::xkb;


fn check_layout(name: &str) {
    let layout = Layout::from_resource(name)
        .and_then(|layout| layout.build().map_err(LoadError::BadKeyMap))
        .expect("layout broken");
    
    let context = xkb::Context::new(xkb::CONTEXT_NO_FLAGS);
    
    let keymap_str = layout.keymap_str
        .clone()
        .into_string().expect("Failed to decode keymap string");
    
    let keymap = xkb::Keymap::new_from_string(
        &context,
        keymap_str.clone(),
        xkb::KEYMAP_FORMAT_TEXT_V1,
        xkb::KEYMAP_COMPILE_NO_FLAGS,
    ).expect("Failed to create keymap");

    let state = xkb::State::new(&keymap);
    
    // "Press" each button with keysyms
    for view in layout.views.values() {
        for row in &view.rows {
            for button in &row.buttons {
                let keystate = button.state.borrow();
                for keycode in &keystate.keycodes {
                    match state.key_get_one_sym(*keycode) {
                        xkb::KEY_NoSymbol => {
                            eprintln!("{}", keymap_str);
                            panic!("Keysym {} on key {:?} can't be resolved", keycode, button.name);
                        },
                        _ => {},
                    }
                }
            }
        }
    }
}

fn main() -> () {
    check_layout(env::args().nth(1).expect("No argument given").as_str());
}
