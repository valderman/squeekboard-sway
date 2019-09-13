extern crate rs;
extern crate xkbcommon;

use std::env;

use rs::data::{ load_layout_from_resource, LoadError };

use xkbcommon::xkb;


fn check_layout(name: &str) {
    let layout = load_layout_from_resource(name)
        .and_then(|layout| layout.build().map_err(LoadError::BadKeyMap))
        .expect("layout broken");
    
    let context = xkb::Context::new(xkb::CONTEXT_NO_FLAGS);
    xkb::Keymap::new_from_string(
        &context,
        layout.keymap_str
            .clone()
            .into_string().expect("Failed to decode keymap string"),
        xkb::KEYMAP_FORMAT_TEXT_V1,
        xkb::KEYMAP_COMPILE_NO_FLAGS,
    ).expect("Failed to create keymap");
}

fn main() -> () {
    check_layout(env::args().nth(1).expect("No argument given").as_str());
}
