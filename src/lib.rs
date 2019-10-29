#[macro_use]
extern crate bitflags;
#[allow(unused_imports)]
#[macro_use] // only for tests
extern crate maplit;
extern crate serde;
extern crate xkbcommon;

mod action;
pub mod data;
pub mod float_ord;
pub mod imservice;
mod keyboard;
mod layout;
mod outputs;
mod resources;
mod submission;
mod util;
mod xdg;
