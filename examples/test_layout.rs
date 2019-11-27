extern crate rs;

use rs::tests::check_builtin_layout;
use std::env;

fn main() -> () {
    check_builtin_layout(
        env::args().nth(1).expect("No argument given").as_str()
    );
}
