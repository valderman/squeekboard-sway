extern crate rs;

use rs::tests::check_layout_file;
use std::env;

fn main() -> () {
    check_layout_file(env::args().nth(1).expect("No argument given").as_str());
}
