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
 
use ::action::Action;
use ::imservice;
use ::imservice::IMService;
use ::keyboard::{ KeyCode, KeyState, KeyStateId, PressType };
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

impl Submission {
    /// Sends a submit text event if possible;
    /// otherwise sends key press and makes a note of it
    pub fn handle_press(
        &mut self,
        key: &KeyState, key_id: KeyStateId,
        time: Timestamp,
    ) {
        let key_string = match &key.action {
            Action::Submit { text, keys: _ } => text,
            _ => {
                eprintln!("BUG: Submitted key with action other than Submit");
                return;
            },
        };

        let text_was_committed = match (&mut self.imservice, key_string) {
            (Some(imservice), Some(text)) => {
                let submit_result = imservice.commit_string(text)
                    .and_then(|_| imservice.commit());
                match submit_result {
                    Ok(()) => true,
                    Err(imservice::SubmitError::NotActive) => false,
                }
            },
            (_, _) => false,
        };
        
        let submit_action = match text_was_committed {
            true => SubmittedAction::IMService,
            false => {
                self.virtual_keyboard.switch(
                    &key.keycodes,
                    PressType::Pressed,
                    time,
                );
                SubmittedAction::VirtualKeyboard(key.keycodes.clone())
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
