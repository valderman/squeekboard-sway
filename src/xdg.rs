/*! XDG directory handling. */

/* Based on dirs-sys https://github.com/soc/dirs-sys-rs
 * by "Simon Ochsenreither <simon@ochsenreither.de>",
 * Licensed under either of

    Apache License, Version 2.0 (LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0)
    MIT license (LICENSE-MIT or http://opensource.org/licenses/MIT)

at your option.
 * 
 * Based on dirs https://github.com/soc/dirs-rs
 * by "Simon Ochsenreither <simon@ochsenreither.de>",
 * Licensed under either of

    Apache License, Version 2.0 (LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0)
    MIT license (LICENSE-MIT or http://opensource.org/licenses/MIT)

at your option.
 * 
 * Based on xdg https://github.com/whitequark/rust-xdg
 * by "Ben Longbons <b.r.longbons@gmail.com>",
 *    "whitequark <whitequark@whitequark.org>",
rust-xdg is distributed under the terms of both the MIT license and the Apache License (Version 2.0).
 * 
 * The above crates were used to get
 * a version without all the excessive dependencies.
 */

use std::env;
use std::ffi::OsString;
use std::path::{ Path, PathBuf };


fn is_absolute_path(path: OsString) -> Option<PathBuf> {
    let path = PathBuf::from(path);
    if path.is_absolute() {
        Some(path)
    } else {
        None
    }
}

fn home_dir() -> Option<PathBuf> {
    return env::var_os("HOME")
        .and_then(|h| if h.is_empty() { None } else { Some(h) })
        .map(PathBuf::from);
}

fn data_dir() -> Option<PathBuf> {
    env::var_os("XDG_DATA_HOME")
        .and_then(is_absolute_path)
        .or_else(|| home_dir().map(|h| h.join(".local/share")))
}

/// Returns the path to the directory within the data dir
pub fn data_path<P>(path: P) -> Option<PathBuf>
    where P: AsRef<Path>
{
    data_dir().map(|dir| {
        dir.join(path.as_ref())
    })
}
