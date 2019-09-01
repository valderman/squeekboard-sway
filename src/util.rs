pub mod c {
    use std::ffi::{ CStr, CString };
    use std::os::raw::c_char;
    use std::str::Utf8Error;
    
    pub fn as_str(s: &*const c_char) -> Result<Option<&str>, Utf8Error> {
        if s.is_null() {
            Ok(None)
        } else {
            unsafe {CStr::from_ptr(*s)}
                .to_str()
                .map(Some)
        }
    }

    pub fn as_cstr(s: &*const c_char) -> Option<&CStr> {
        if s.is_null() {
            None
        } else {
            Some(unsafe {CStr::from_ptr(*s)})
        }
    }

    pub fn into_cstring(s: *const c_char) -> Result<Option<CString>, std::ffi::NulError> {
        if s.is_null() {
            Ok(None)
        } else {
            CString::new(
                unsafe {CStr::from_ptr(s)}.to_bytes()
            ).map(Some)
        }
    }
    
    #[cfg(test)]
    mod tests {
        use super::*;
        use std::ptr;
        
        #[test]
        fn test_null_cstring() {
            assert_eq!(into_cstring(ptr::null()), Ok(None))
        }
        
        #[test]
        fn test_null_str() {
            assert_eq!(as_str(&ptr::null()), Ok(None))
        }
    }
}
