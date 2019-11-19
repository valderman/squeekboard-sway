#[macro_use]
extern crate bitflags;
extern crate gio;
extern crate glib;
extern crate glib_sys;
extern crate gtk;
extern crate gtk_sys;
#[allow(unused_imports)]
#[macro_use] // only for tests
extern crate maplit;
extern crate regex;
extern crate serde;
extern crate xkbcommon;

mod action;
pub mod data;
pub mod float_ord;
pub mod imservice;
mod keyboard;
mod layout;
mod locale;
mod locale_config;
mod outputs;
mod popover;
mod resources;
mod submission;
pub mod tests;
pub mod util;
mod xdg;
