/*! Managing the state of text input in the application.
 * 
 * This is a library module.
 * 
 * It needs to combine text-input and virtual-keyboard protocols
 * to achieve a consistent view of the text-input state,
 * and to submit exactly what the user wanted.
 * 
 * It must also not get tripped up by sudden disappearances of interfaces.
 * 
 * The virtual-keyboard interface is always present.
 * 
 * The text-input interface may not be presented,
 * and, for simplicity, no further attempt to claim it is made.
 * 
 * The text-input interface may be enabled and disabled at arbitrary times,
 * and those events SHOULD NOT cause any lost events.
 * */

/// Temporary reexport to keep stuff based directly on virtual-keyboard working
/// until a unified handler appears and prompts a rework.
pub use vkeyboard::*;

use ::imservice::IMService;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    
    use std::os::raw::c_void;

    use ::imservice::c::InputMethod;

    // The following defined in C

    /// ServerContextService*
    #[repr(transparent)]
    pub struct UIManager(*const c_void);

    /// EekboardContextService*
    #[repr(transparent)]
    pub struct StateManager(*const c_void);

    #[no_mangle]
    pub extern "C"
    fn submission_new(
        im: *mut InputMethod,
        state_manager: *const StateManager
    ) -> *mut Submission {
        let imservice = if im.is_null() {
            None
        } else {
            Some(IMService::new(im, state_manager))
        };
        // TODO: add vkeyboard too
        Box::<Submission>::into_raw(Box::new(
            Submission {
                imservice,
            }
        ))
    }

    #[no_mangle]
    pub extern "C"
    fn submission_set_ui(submission: *mut Submission, ui_manager: *const UIManager) {
        if submission.is_null() {
            panic!("Null submission pointer");
        }
        let submission: &mut Submission = unsafe { &mut *submission };
        if let Some(ref mut imservice) = &mut submission.imservice {
            imservice.ui_manager = if ui_manager.is_null() {
                None
            } else {
                Some(ui_manager)
            }
        };
    }
}

pub struct Submission {
    // used by C callbacks internally, TODO: make use with virtual keyboard
    #[allow(dead_code)]
    imservice: Option<Box<IMService>>,
}
