/*! Procedures relating to the management of the switching of layouts */

pub mod c {
    use std::os::raw::{c_char, c_void};

    /// EekboardContextService*
    #[repr(transparent)]
    #[derive(Clone, Copy)]
    pub struct Manager(*const c_void);
    
    #[no_mangle]
    extern "C" {
        pub fn eekboard_context_service_set_overlay(
            manager: Manager,
            name: *const c_char,
        );
    }
}
