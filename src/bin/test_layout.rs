#[macro_use]
extern crate clap;
extern crate rs;

use rs::tests::check_layout_file;

fn main() -> () {
    let matches = clap_app!(test_layout =>
        (name: "squeekboard-test-layout")
        (about: "Test keyboard layout for errors. Returns OK or an error message containing further information.")
        (@arg INPUT: +required "Yaml keyboard layout file to test")
    ).get_matches();
    if check_layout_file(matches.value_of("INPUT").unwrap()) == () {
        println!("Test result: OK");
    }
}
