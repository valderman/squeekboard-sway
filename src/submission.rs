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

use std::collections::HashSet;
use std::ffi::CString;
use ::action::Modifier;
use ::imservice;
use ::imservice::IMService;
use ::keyboard::{ KeyCode, KeyStateId, Modifiers, PressType };
use ::layout::c::LevelKeyboard;
use ::util::vec_remove;
use ::vkeyboard::VirtualKeyboard;

// traits
use std::iter::FromIterator;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    
    use std::os::raw::c_void;

    use ::imservice::c::InputMethod;
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
                modifiers_active: Vec::new(),
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
    fn submission_set_keyboard(
        submission: *mut Submission,
        keyboard: LevelKeyboard,
        time: u32,
    ) {
        if submission.is_null() {
            panic!("Null submission pointer");
        }
        let submission: &mut Submission = unsafe { &mut *submission };
        submission.update_keymap(keyboard, Timestamp(time));
    }
}

#[derive(Clone, Copy)]
pub struct Timestamp(pub u32);

#[derive(Clone)]
enum SubmittedAction {
    /// A collection of keycodes that were pressed
    VirtualKeyboard(Vec<KeyCode>),
    IMService,
}

pub struct Submission {
    imservice: Option<Box<IMService>>,
    virtual_keyboard: VirtualKeyboard,
    modifiers_active: Vec<(KeyStateId, Modifier)>,
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
        let mods_are_on = !self.modifiers_active.is_empty();

        let was_committed_as_text = match (&mut self.imservice, mods_are_on) {
            (Some(imservice), false) => {
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
            (_, _) => false,
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

        self.modifiers_active.truncate(0);
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
    
    pub fn handle_add_modifier(
        &mut self,
        key_id: KeyStateId,
        modifier: Modifier, _time: Timestamp,
    ) {
        self.modifiers_active.push((key_id, modifier));
        self.update_modifiers();
    }

    pub fn handle_drop_modifier(
        &mut self,
        key_id: KeyStateId,
        _time: Timestamp,
    ) {
        vec_remove(&mut self.modifiers_active, |(id, _)| *id == key_id);
        self.update_modifiers();
    }

    fn update_modifiers(&mut self) {
        let raw_modifiers = self.modifiers_active.iter()
            .map(|(_id, m)| match m {
                Modifier::Control => Modifiers::CONTROL,
                Modifier::Alt => Modifiers::MOD1,
                Modifier::Mod4 => Modifiers::MOD4,
                Modifier::Shift => Modifiers::SHIFT,
            })
            .fold(Modifiers::empty(), |m, n| m | n);
        self.virtual_keyboard.set_modifiers_state(raw_modifiers);
    }

    pub fn is_modifier_active(&self, modifier: Modifier) -> bool {
        self.modifiers_active.iter()
            .position(|(_id, m)| *m == modifier)
            .is_some()
    }

    pub fn get_active_modifiers(&self) -> HashSet<Modifier> {
        HashSet::from_iter(
            self.modifiers_active.iter().map(|(_id, m)| m.clone())
        )
    }

    fn clear_all_modifiers(&mut self) {
        // Looks like an optimization,
        // but preemptive cleaning is needed before setting a new keymap,
        // so removing this check would break keymap setting.
        if self.modifiers_active.is_empty() {
            return;
        }
        self.modifiers_active = Vec::new();
        self.virtual_keyboard.set_modifiers_state(Modifiers::empty())
    }

    fn release_all_virtual_keys(&mut self, time: Timestamp) {
        let virtual_pressed = self.pressed
            .clone().into_iter()
            .filter_map(|(id, action)| {
                match action {
                    SubmittedAction::VirtualKeyboard(_) => Some(id),
                    _ => None,
                }
            });
        for id in virtual_pressed {
            self.handle_release(id, time);
        }
    }

    /// Changes keymap and clears pressed keys and modifiers.
    ///
    /// It's not obvious if clearing is the right thing to do, 
    /// but keymap update may (or may not) do that,
    /// possibly putting self.modifiers_active and self.pressed out of sync,
    /// so a consistent stance is adopted to avoid that.
    /// Alternatively, modifiers could be restored on the new keymap.
    /// That approach might be difficult
    /// due to modifiers meaning different things in different keymaps.
    pub fn update_keymap(&mut self, keyboard: LevelKeyboard, time: Timestamp) {
        self.clear_all_modifiers();
        self.release_all_virtual_keys(time);
        self.virtual_keyboard.update_keymap(keyboard);
    }
}
