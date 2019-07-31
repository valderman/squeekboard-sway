use std::boxed::Box;
use std::ffi::CString;
use std::num::Wrapping;
use std::string::String;

use super::bitflags;

/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    
    use std::ffi::CStr;
    use std::os::raw::{c_char, c_void};
    
    fn into_cstring(s: *const c_char) -> Result<CString, std::ffi::NulError> {
        CString::new(
            unsafe {CStr::from_ptr(s)}.to_bytes()
        )
    }
    
    // The following defined in C
        
    /// struct zwp_input_method_v2*
    #[repr(transparent)]
    pub struct InputMethod(*const c_void);
    
    /// EekboardContextService*
    #[repr(transparent)]
    pub struct UIManager(*const c_void);
    
    #[no_mangle]
    extern "C" {
        fn imservice_destroy_im(im: *mut c::InputMethod);
        fn eekboard_context_service_set_hint_purpose(imservice: *const UIManager, hint: u32, purpose: u32);
        fn eekboard_context_service_show_keyboard(imservice: *const UIManager);
        fn eekboard_context_service_hide_keyboard(imservice: *const UIManager);
    }
    
    // The following defined in Rust. TODO: wrap naked pointers to Rust data inside RefCells to prevent multiple writers
    
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_new(im: *const InputMethod, ui_manager: *const UIManager) -> *mut IMService {
        Box::<IMService>::into_raw(Box::new(
            IMService {
                im: im,
                ui_manager: ui_manager,
                pending: IMProtocolState::default(),
                current: IMProtocolState::default(),
                preedit_string: String::new(),
                serial: Wrapping(0u32),
            }
        ))
    }
    
    // TODO: is unsafe needed here?
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_handle_input_method_activate(imservice: *mut IMService,
        _im: *const InputMethod)
    {
        let imservice = &mut *imservice;
        imservice.preedit_string = String::new();
        imservice.pending = IMProtocolState {
            active: true,
            ..IMProtocolState::default()
        };
    }
    
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_handle_input_method_deactivate(imservice: *mut IMService,
        _im: *const InputMethod)
    {
        let imservice = &mut *imservice;
        imservice.pending = IMProtocolState {
            active: false,
            ..imservice.pending.clone()
        };
    }
    
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_handle_surrounding_text(imservice: *mut IMService,
        _im: *const InputMethod,
        text: *const c_char, cursor: u32, _anchor: u32)
    {
        let imservice = &mut *imservice;
        imservice.pending = IMProtocolState {
            surrounding_text: into_cstring(text).expect("Received invalid string"),
            surrounding_cursor: cursor,
            ..imservice.pending.clone()
        };
    }
    
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_handle_content_type(imservice: *mut IMService,
        _im: *const InputMethod,
        hint: u32, purpose: u32)
    {
        let imservice = &mut *imservice;
        imservice.pending = IMProtocolState {
            content_hint: {
                ContentHint::from_bits(hint).unwrap_or_else(|| {
                    eprintln!("Warning: received invalid hint flags");
                    ContentHint::NONE
                })
            },
            content_purpose: {
                ContentPurpose::from_num(purpose).unwrap_or_else(|| {
                    eprintln!("Warning: Received invalid purpose flags");
                    ContentPurpose::Normal
                })
            },
            ..imservice.pending.clone()
        };
    }
    
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_handle_text_change_cause(imservice: *mut IMService,
        _im: *const InputMethod,
        cause: u32)
    {
        let imservice = &mut *imservice;
        imservice.pending = IMProtocolState {
            text_change_cause: {
                ChangeCause::from_num(cause).unwrap_or_else(|| {
                    eprintln!("Warning: received invalid cause flags");
                    ChangeCause::InputMethod
                })
            },
            ..imservice.pending.clone()
        };
    }
    
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_handle_commit_state(imservice: *mut IMService,
        _im: *const InputMethod)
    {
        let imservice = &mut *imservice;
        let active_changed = imservice.current.active ^ imservice.pending.active;
        
        imservice.serial += Wrapping(1u32);
        imservice.current = imservice.pending.clone();
        imservice.pending = IMProtocolState {
            active: imservice.current.active,
            ..IMProtocolState::default()
        };
        if active_changed {
            if imservice.current.active {
                eekboard_context_service_show_keyboard(imservice.ui_manager);
                eekboard_context_service_set_hint_purpose(
                    imservice.ui_manager,
                    imservice.current.content_hint.bits(),
                    imservice.current.content_purpose.as_num());
            } else {
                eekboard_context_service_hide_keyboard(imservice.ui_manager);
            }
        }
    }
    
    #[no_mangle]
    pub unsafe extern "C"
    fn imservice_handle_unavailable(imservice: *mut IMService,
        im: *mut InputMethod)
    {
        imservice_destroy_im(im);

        let imservice = &mut *imservice;

        // no need to care about proper double-buffering,
        // the keyboard is already decommissioned
        imservice.current.active = false;

        eekboard_context_service_hide_keyboard(imservice.ui_manager);
    }

    // FIXME: destroy and deallocate
}


bitflags!{
    /// Map to `text_input_unstable_v3.content_hint` values
    pub struct ContentHint: u32 {
        const NONE = 0x0;
        const COMPLETION = 0x1;
        const SPELLCHECK = 0x2;
        const AUTO_CAPITALIZATION = 0x4;
        const LOWERCASE = 0x8;
        const UPPERCASE = 0x10;
        const TITLECASE = 0x20;
        const HIDDEN_TEXT = 0x40;
        const SENSITIVE_DATA = 0x80;
        const LATIN = 0x100;
        const MULTILINE = 0x200;
    }
}

/// Map to `text_input_unstable_v3.content_purpose` values
#[derive(Debug, Clone)]
pub enum ContentPurpose {
    Normal,
    Alpha,
    Digits,
    Number,
    Phone,
    Url,
    Email,
    Name,
    Password,
    Pin,
    Date,
    Time,
    Datetime,
    Terminal,
}

impl ContentPurpose {
    fn from_num(num: u32) -> Option<ContentPurpose> {
        use self::ContentPurpose::*;
        match num {
            0 => Some(Normal),
            1 => Some(Alpha),
            2 => Some(Digits),
            3 => Some(Number),
            4 => Some(Phone),
            5 => Some(Url),
            6 => Some(Email),
            7 => Some(Name),
            8 => Some(Password),
            9 => Some(Pin),
            10 => Some(Date),
            11 => Some(Time),
            12 => Some(Datetime),
            13 => Some(Terminal),
            _ => None,
        }
    }
    fn as_num(self: &ContentPurpose) -> u32 {
        use self::ContentPurpose::*;
        match self {
            Normal => 0,
            Alpha => 1,
            Digits => 2,
            Number => 3,
            Phone => 4,
            Url => 5,
            Email => 6,
            Name => 7,
            Password => 8,
            Pin => 9,
            Date => 10,
            Time => 11,
            Datetime => 12,
            Terminal => 13,
        }
    }
}

/// Map to `text_input_unstable_v3.change_cause` values
#[derive(Debug, Clone)]
pub enum ChangeCause {
    InputMethod,
    Other,
}

impl ChangeCause {
    pub fn from_num(num: u32) -> Option<ChangeCause> {
        match num {
            0 => Some(ChangeCause::InputMethod),
            1 => Some(ChangeCause::Other),
            _ => None
        }
    }
    pub fn as_num(&self) -> u32 {
        match &self {
            ChangeCause::InputMethod => 0,
            ChangeCause::Other => 1,
        }
    }
}

/// Describes the desired state of the input method as requested by the server
#[derive(Clone)]
struct IMProtocolState {
    surrounding_text: CString,
    surrounding_cursor: u32,
    content_purpose: ContentPurpose,
    content_hint: ContentHint,
    text_change_cause: ChangeCause,
    active: bool,
}

impl Default for IMProtocolState {
    fn default() -> IMProtocolState {
        IMProtocolState {
            surrounding_text: CString::default(),
            surrounding_cursor: 0, // TODO: mark that there's no cursor
            content_hint: ContentHint::NONE,
            content_purpose: ContentPurpose::Normal,
            text_change_cause: ChangeCause::InputMethod,
            active: false,
        }
    }
}

pub struct IMService {
    /// Owned reference (still created and destroyed in C)
    im: *const c::InputMethod,
    /// Unowned reference. Be careful, it's shared with C at large
    ui_manager: *const c::UIManager,

    pending: IMProtocolState,
    current: IMProtocolState, // turn current into an idiomatic representation?
    preedit_string: String,
    serial: Wrapping<u32>,
}
