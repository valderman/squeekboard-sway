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
 
use std::ffi::CString;
 
use ::imservice;
use ::imservice::IMService;
use ::keyboard::{ KeyCode, KeyStateId, PressType };
use ::vkeyboard::VirtualKeyboard;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    
    use std::os::raw::c_void;

    use ::imservice::c::InputMethod;
    use ::layout::c::LevelKeyboard;
    use ::vkeyboard::c::ZwpVirtualKeyboardV1;

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
        vk: ZwpVirtualKeyboardV1,
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
                virtual_keyboard: VirtualKeyboard(vk),
                pressed: Vec::new(),
            }
        ))
    }

    /// Use to initialize the UI reference
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

    #[no_mangle]
    pub extern "C"
    fn submission_set_keyboard(submission: *mut Submission, keyboard: LevelKeyboard) {
        if submission.is_null() {
            panic!("Null submission pointer");
        }
        let submission: &mut Submission = unsafe { &mut *submission };
        submission.virtual_keyboard.update_keymap(keyboard);
    }
}

#[derive(Clone, Copy)]
pub struct Timestamp(pub u32);

enum SubmittedAction {
    /// A collection of keycodes that were pressed
    VirtualKeyboard(Vec<KeyCode>),
    IMService,
}

pub struct Submission {
    imservice: Option<Box<IMService>>,
    virtual_keyboard: VirtualKeyboard,
    pressed: Vec<(KeyStateId, SubmittedAction)>,
}

pub enum SubmitData<'a> {
    Text(&'a CString),
    Erase,
    Keycodes,
}

impl Submission {
    /// Sends a submit text event if possible;
    /// otherwise sends key press and makes a note of it
    pub fn handle_press(
        &mut self,
        key_id: KeyStateId,
        data: SubmitData,
        keycodes: &Vec<KeyCode>,
        time: Timestamp,
    ) {
        let was_committed_as_text = match &mut self.imservice {
            Some(imservice) => {
                enum Outcome {
                    Submitted(Result<(), imservice::SubmitError>),
                    NotSubmitted,
                };

                let submit_outcome = match data {
                    SubmitData::Text(text) => {
                        Outcome::Submitted(imservice.commit_string(text))
                    },
                    SubmitData::Erase => {
                        /* Delete_surrounding_text takes byte offsets,
                         * so cannot work without get_surrounding_text.
                         * This is a bug in the protocol.
                         */
                        // imservice.delete_surrounding_text(1, 0),
                        Outcome::NotSubmitted
                    },
                    SubmitData::Keycodes => Outcome::NotSubmitted,
                };

                match submit_outcome {
                    Outcome::Submitted(result) => {
                        match result.and_then(|()| imservice.commit()) {
                            Ok(()) => true,
                            Err(imservice::SubmitError::NotActive) => false,
                        }
                    },
                    Outcome::NotSubmitted => false,
                }
            },
            _ => false,
        };

        let submit_action = match was_committed_as_text {
            true => SubmittedAction::IMService,
            false => {
                self.virtual_keyboard.switch(
                    keycodes,
                    PressType::Pressed,
                    time,
                );
                SubmittedAction::VirtualKeyboard(keycodes.clone())
            },
        };
        
        self.pressed.push((key_id, submit_action));
    }
    
    pub fn handle_release(&mut self, key_id: KeyStateId, time: Timestamp) {
        let index = self.pressed.iter().position(|(id, _)| *id == key_id);
        if let Some(index) = index {
            let (_id, action) = self.pressed.remove(index);
            match action {
                // string already sent, nothing to do
                SubmittedAction::IMService => {},
                // no matter if the imservice got activated,
                // keys must be released
                SubmittedAction::VirtualKeyboard(keycodes) => {
                    self.virtual_keyboard.switch(
                        &keycodes,
                        PressType::Released,
                        time,
                    )
                },
            }
        };
    }
}
