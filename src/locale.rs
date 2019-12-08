/*! Locale-specific functions */

use std::cmp;
use std::ffi::CString;

mod c {
    use std::os::raw::c_char;

    #[allow(non_camel_case_types)]
    pub type c_int = i32;

    #[no_mangle]
    extern "C" {
        // from libc
        pub fn strcoll(cs: *const c_char, ct: *const c_char) -> c_int;
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Translation<'a>(pub &'a str);

fn cstring_safe(s: &str) -> CString {
    CString::new(s)
        .unwrap_or(CString::new("").unwrap())
}

pub fn compare_current_locale(a: &str, b: &str) -> cmp::Ordering {
    let a = cstring_safe(a);
    let b = cstring_safe(b);
    let a = a.as_ptr();
    let b = b.as_ptr();
    let result = unsafe { c::strcoll(a, b) };
    if result == 0 {
        cmp::Ordering::Equal
    } else if result > 0 {
        cmp::Ordering::Greater
    } else if result < 0 {
        cmp::Ordering::Less
    } else {
        unreachable!()
    }
}
