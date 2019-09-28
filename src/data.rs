/**! The parsing of the data files for layouts */

use std::cell::RefCell;
use std::collections::{ HashMap, HashSet };
use std::env;
use std::ffi::CString;
use std::fmt;
use std::fs;
use std::io;
use std::path::PathBuf;
use std::rc::Rc;
use std::vec::Vec;

use xkbcommon::xkb;

use ::keyboard::{
    KeyState,
    generate_keymap, generate_keycodes, FormattingError
};
use ::resources;
use ::util::c::as_str;
use ::xdg;

// traits, derives
use std::io::BufReader;
use std::iter::FromIterator;

use serde::Deserialize;


/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    use std::os::raw::c_char;
    
    #[no_mangle]
    pub extern "C"
    fn squeek_load_layout(name: *const c_char) -> *mut ::layout::Layout {
        let name = as_str(&name)
            .expect("Bad layout name")
            .expect("Empty layout name");

        let layout = build_layout_with_fallback(name);
        Box::into_raw(Box::new(layout))
    }
}

const FALLBACK_LAYOUT_NAME: &str = "us";

#[derive(Debug)]
pub enum LoadError {
    BadData(Error),
    MissingResource,
    BadResource(serde_yaml::Error),
    BadKeyMap(FormattingError),
}

impl fmt::Display for LoadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use self::LoadError::*;
        match self {
            BadData(e) => write!(f, "Bad data: {}", e),
            MissingResource => write!(f, "Missing resource"),
            BadResource(e) => write!(f, "Bad resource: {}", e),
            BadKeyMap(e) => write!(f, "Bad key map: {}", e),
        }
    }
}

pub fn load_layout_from_resource(
    name: &str
) -> Result<Layout, LoadError> {
    let data = resources::get_keyboard(name)
                .ok_or(LoadError::MissingResource)?;
    serde_yaml::from_str(data)
                .map_err(|e| LoadError::BadResource(e))
}

#[derive(Debug, PartialEq)]
enum DataSource {
    File(PathBuf),
    Resource(String),
}

impl fmt::Display for DataSource {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DataSource::File(path) => write!(f, "Path: {:?}", path.display()),
            DataSource::Resource(name) => write!(f, "Resource: {}", name),
        }
    }
}

/// Tries to load the layout from the first place where it's present.
/// If the layout exists, but is broken, fallback is activated.
fn load_layout(
    name: &str,
    keyboards_path: Option<PathBuf>,
) -> (
    Result<Layout, LoadError>, // last attempted
    DataSource, // last attempt source
    Option<(LoadError, DataSource)>, // first attempt source
) {
    let path = keyboards_path.map(|path| path.join(name).with_extension("yaml"));

    let layout = match path {
        Some(path) => Some((
            Layout::from_yaml_stream(path.clone())
                .map_err(LoadError::BadData),
            DataSource::File(path),
        )),
        None => None, // No env var, not an error
    };

    let (failed_attempt, layout) = match layout {
        Some((Ok(layout), path)) => (None, Some((layout, path))),
        Some((Err(e), path)) => (Some((e, path)), None),
        None => (None, None),
    };
    
    let (layout, source) = match layout {
        Some((layout, path)) => (Ok(layout), path),
        None => (
            load_layout_from_resource(name),
            DataSource::Resource(name.into()),
        ),
    };

    (layout, source, failed_attempt)
}

fn build_layout(
    name: &str,
    keyboards_path: Option<PathBuf>,
) -> (
    Result<::layout::Layout, LoadError>, // last attempted
    DataSource, // last attempt source
    Option<(LoadError, DataSource)>, // first attempt source
) {
    let (layout, source, failed_attempt) = load_layout(name, keyboards_path);
    
    // FIXME: attempt at each step of fallback
    let layout = layout.and_then(
        |layout| layout.build().map_err(LoadError::BadKeyMap)
    );

    (layout, source, failed_attempt)
}

fn build_layout_with_fallback(
    name: &str
) -> ::layout::Layout {
    let path = env::var_os("SQUEEKBOARD_KEYBOARDSDIR")
        .map(PathBuf::from)
        .or_else(|| xdg::data_path("squeekboard/keyboards"));
    
    let (layout, source, attempt) = build_layout(name, path.clone());
    
    if let Some((e, source)) = attempt {
        eprintln!(
            "Failed to load layout from {}: {}, trying builtin",
            source, e
        );
    };
    
    let (layout, source, attempt) = match (layout, source) {
        (Err(e), source) => {
            eprintln!(
                "Failed to load layout from {}: {}, using fallback",
                source, e
            );
            build_layout(FALLBACK_LAYOUT_NAME, path)
        },
        (res, source) => (res, source, None),
    };

    if let Some((e, source)) = attempt {
        eprintln!(
            "Failed to load layout from {}: {}, trying builtin",
            source, e
        );
    };

    match (layout, source) {
        (Err(e), source) => {
            panic!(
                format!("Failed to load hardcoded layout from {}: {:?}",
                    source, e
                )
            );
        },
        (Ok(layout), source) => {
            eprintln!("Loaded layout from {}", source);
            layout
        }
    }
}

/// The root element describing an entire keyboard
#[derive(Debug, Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
pub struct Layout {
    row_spacing: f64,
    button_spacing: f64,
    bounds: Bounds,
    views: HashMap<String, Vec<ButtonIds>>,
    #[serde(default)] 
    buttons: HashMap<String, ButtonMeta>,
    outlines: HashMap<String, Outline>
}

#[derive(Debug, Clone, Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
struct Bounds {
    x: f64,
    y: f64,
    width: f64,
    height: f64,
}

/// Buttons are embedded in a single string
type ButtonIds = String;

#[derive(Debug, Default, Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
struct ButtonMeta {
    /// Action other than keysym (conflicts with keysym)
    action: Option<Action>,
    /// The name of the outline. If not present, will be "default"
    outline: Option<String>,
    /// FIXME: start using it
    keysym: Option<String>,
    /// If not present, will be derived from the button ID
    label: Option<String>,
    /// Conflicts with label
    icon: Option<String>,
}

#[derive(Debug, Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
enum Action {
    #[serde(rename="locking")]
    Locking { lock_view: String, unlock_view: String },
    #[serde(rename="set_view")]
    SetView(String),
    #[serde(rename="show_prefs")]
    ShowPrefs,
}

#[derive(Debug, Clone, Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
struct Outline {
    bounds: Bounds,
}

#[derive(Debug)]
pub enum Error {
    Yaml(serde_yaml::Error),
    Io(io::Error),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Yaml(e) => write!(f, "YAML: {}", e),
            Error::Io(e) => write!(f, "IO: {}", e),
        }
    }
}

impl Layout {
    fn from_yaml_stream(path: PathBuf) -> Result<Layout, Error> {
        let infile = BufReader::new(
            fs::OpenOptions::new()
                .read(true)
                .open(&path)
                .map_err(Error::Io)?
        );
        serde_yaml::from_reader(infile)
            .map_err(Error::Yaml)
    }
    pub fn build(self) -> Result<::layout::Layout, FormattingError> {
        let button_names = self.views.values()
            .flat_map(|rows| {
                rows.iter()
                    .flat_map(|row| row.split_ascii_whitespace())
            });
        
        let button_names: HashSet<&str>
            = HashSet::from_iter(button_names);

        let keycodes = generate_keycodes(
            button_names.iter()
                .map(|name| *name)
                .filter(|name| {
                    match self.buttons.get(*name) {
                        // buttons with defined action can't emit keysyms
                        // and so don't need keycodes
                        Some(ButtonMeta { action: Some(_), .. }) => false,
                        _ => true,
                    }
                })
        );

        let button_states = button_names.iter().map(|name| {(
            String::from(*name),
            Rc::new(RefCell::new(KeyState {
                pressed: false,
                locked: false,
                keycode: keycodes.get(*name).map(|k| *k),
                symbol: create_symbol(
                    &self.buttons,
                    name,
                    self.views.keys().collect()
                ),
            }))
        )});

        let button_states =
            HashMap::<String, Rc<RefCell<KeyState>>>::from_iter(
                button_states
            );

        // TODO: generate from symbols
        let keymap_str = generate_keymap(&button_states)?;

        let views = HashMap::from_iter(
            self.views.iter().map(|(name, view)| {(
                name.clone(),
                Box::new(::layout::View {
                    bounds: ::layout::c::Bounds {
                        x: self.bounds.x,
                        y: self.bounds.y,
                        width: self.bounds.width,
                        height: self.bounds.height,
                    },
                    spacing: ::layout::Spacing {
                        row: self.row_spacing,
                        button: self.button_spacing,
                    },
                    rows: view.iter().map(|row| {
                        Box::new(::layout::Row {
                            angle: 0,
                            bounds: None,
                            buttons: row.split_ascii_whitespace().map(|name| {
                                Box::new(create_button(
                                    &self.buttons,
                                    &self.outlines,
                                    name,
                                    button_states.get(name.into())
                                        .expect("Button state not created")
                                        .clone()
                                ))
                            }).collect(),
                        })
                    }).collect(),
                })
            )})
        );

        Ok(::layout::Layout {
            current_view: "base".into(),
            views: views,
            keymap_str: {
                CString::new(keymap_str)
                    .expect("Invalid keymap string generated")
            },
        })
    }
}

fn create_symbol(
    button_info: &HashMap<String, ButtonMeta>,
    name: &str,
    view_names: Vec<&String>,
) -> ::symbol::Symbol {
    let default_meta = ButtonMeta::default();
    let symbol_meta = button_info.get(name)
        .unwrap_or(&default_meta);
    
    fn filter_view_name(
        button_name: &str,
        view_name: String,
        view_names: &Vec<&String>
    ) -> String {
        if view_names.contains(&&view_name) {
            view_name
        } else {
            eprintln!(
                "Button {} switches to missing view {}",
                button_name,
                view_name
            );
            "base".into()
        }
    }
    
    fn keysym_valid(name: &str) -> bool {
        xkb::keysym_from_name(name, xkb::KEYSYM_NO_FLAGS) != xkb::KEY_NoSymbol
    }
    
    let keysym = match &symbol_meta.action {
        Some(_) => None,
        None => Some(match &symbol_meta.keysym {
            Some(keysym) => match keysym_valid(keysym.as_str()) {
                true => keysym.clone(),
                false => {
                    eprintln!("Keysym name invalid: {}", keysym);
                    "space".into() // placeholder
                },
            },
            None => match keysym_valid(name) {
                true => String::from(name),
                false => match name.chars().count() {
                    1 => format!("U{:04X}", name.chars().next().unwrap() as u32),
                    // If the name is longer than 1 char,
                    // then it's not a single Unicode char,
                    // but was trying to be an identifier
                    _ => {
                        eprintln!(
                            "Could not derive a valid keysym for key {}",
                            name
                        );
                        "space".into() // placeholder
                    }
                },
            },
        }),
    };
    
    match &symbol_meta.action {
        Some(Action::SetView(view_name)) => ::symbol::Symbol {
            action: ::symbol::Action::SetLevel(
                filter_view_name(name, view_name.clone(), &view_names)
            ),
        },
        Some(Action::Locking { lock_view, unlock_view }) => ::symbol::Symbol {
            action: ::symbol::Action::LockLevel {
                lock: filter_view_name(name, lock_view.clone(), &view_names),
                unlock: filter_view_name(
                    name,
                    unlock_view.clone(),
                    &view_names
                ),
            },
        },
        Some(Action::ShowPrefs) => ::symbol::Symbol {
            action: ::symbol::Action::Submit {
                text: None,
                keys: Vec::new(),
            },
        },
        None => ::symbol::Symbol {
            action: ::symbol::Action::Submit {
                text: None,
                keys: vec!(
                    ::symbol::KeySym(keysym.unwrap()),
                ),
            },
        },
    }
}

/// TODO: Since this will receive user-provided data,
/// all .expect() on them should be turned into soft fails
fn create_button(
    button_info: &HashMap<String, ButtonMeta>,
    outlines: &HashMap<String, Outline>,
    name: &str,
    state: Rc<RefCell<KeyState>>,
) -> ::layout::Button {
    let cname = CString::new(name.clone())
        .expect("Bad name");
    // don't remove, because multiple buttons with the same name are allowed
    let default_meta = ButtonMeta::default();
    let button_meta = button_info.get(name)
        .unwrap_or(&default_meta);

    // TODO: move conversion to the C/Rust boundary
    let label = if let Some(label) = &button_meta.label {
        ::layout::Label::Text(CString::new(label.as_str())
            .expect("Bad label"))
    } else if let Some(icon) = &button_meta.icon {
        ::layout::Label::IconName(CString::new(icon.as_str())
            .expect("Bad icon"))
    } else {
        ::layout::Label::Text(cname.clone())
    };

    let outline_name = match &button_meta.outline {
        Some(outline) => {
            if outlines.contains_key(outline) {
                outline.clone()
            } else {
                eprintln!("Outline named {} does not exist! Using default for button {}", outline, name);
                "default".into()
            }
        }
        None => "default".into(),
    };

    let outline = outlines.get(&outline_name)
        .map(|outline| (*outline).clone())
        .unwrap_or_else(|| {
            eprintln!("No default outline defied Using 1x1!");
            Outline {
                bounds: Bounds { x: 0f64, y: 0f64, width: 1f64, height: 1f64 },
            }
        });

    ::layout::Button {
        name: cname,
        // TODO: do layout before creating buttons
        bounds: ::layout::c::Bounds {
            x: outline.bounds.x,
            y: outline.bounds.y,
            width: outline.bounds.width,
            height: outline.bounds.height,
        },
        label: label,
        state: state,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    use std::error::Error as ErrorTrait;

    #[test]
    fn test_parse_path() {
        assert_eq!(
            Layout::from_yaml_stream(
                PathBuf::from("tests/layout.yaml")
            ).unwrap(),
            Layout {
                row_spacing: 0f64,
                button_spacing: 0f64,
                bounds: Bounds { x: 0f64, y: 0f64, width: 0f64, height: 0f64 },
                views: hashmap!(
                    "base".into() => vec!("test".into()),
                ),
                buttons: hashmap!{
                    "test".into() => ButtonMeta {
                        icon: None,
                        keysym: None,
                        action: None,
                        label: Some("test".into()),
                        outline: None,
                    }
                },
                outlines: hashmap!{
                    "default".into() => Outline {
                        bounds: Bounds {
                            x: 0f64, y: 0f64, width: 0f64, height: 0f64
                        }, 
                    }
                },
            }
        );
    }

    /// Check if the default protection works
    #[test]
    fn test_empty_views() {
        let out = Layout::from_yaml_stream(PathBuf::from("tests/layout2.yaml"));
        match out {
            Ok(_) => assert!(false, "Data mistakenly accepted"),
            Err(e) => {
                let mut handled = false;
                if let Error::Yaml(ye) = &e {
                    handled = ye.description() == "missing field `views`";
                };
                if !handled {
                    println!("Unexpected error {:?}", e);
                    assert!(false)
                }
            }
        }
    }

    #[test]
    fn test_extra_field() {
        let out = Layout::from_yaml_stream(PathBuf::from("tests/layout3.yaml"));
        match out {
            Ok(_) => assert!(false, "Data mistakenly accepted"),
            Err(e) => {
                let mut handled = false;
                if let Error::Yaml(ye) = &e {
                    handled = ye.description()
                        .starts_with("unknown field `bad_field`");
                };
                if !handled {
                    println!("Unexpected error {:?}", e);
                    assert!(false)
                }
            }
        }
    }
    
    #[test]
    fn test_layout_punctuation() {
        let out = Layout::from_yaml_stream(PathBuf::from("tests/layout_key1.yaml"))
            .unwrap()
            .build()
            .unwrap();
        assert_eq!(
            out.views["base"]
                .rows[0]
                .buttons[0]
                .label,
            ::layout::Label::Text(CString::new("test").unwrap())
        );
    }

    #[test]
    fn test_layout_unicode() {
        let out = Layout::from_yaml_stream(PathBuf::from("tests/layout_key2.yaml"))
            .unwrap()
            .build()
            .unwrap();
        assert_eq!(
            out.views["base"]
                .rows[0]
                .buttons[0]
                .label,
            ::layout::Label::Text(CString::new("test").unwrap())
        );
    }
    
    #[test]
    fn parsing_fallback() {
        assert!(load_layout_from_resource(FALLBACK_LAYOUT_NAME)
            .and_then(|layout| layout.build().map_err(LoadError::BadKeyMap))
            .is_ok()
        );
    }
    
    /// First fallback should be to builtin, not to FALLBACK_LAYOUT_NAME
    #[test]
    fn fallbacks_order() {
        let (layout, source, _failure) = load_layout(
            "nb",
            Some(PathBuf::from("tests"))
        );
        
        assert_eq!(
            source,
            load_layout("nb", None).1
        );
    }
    
    #[test]
    fn unicode_keysym() {
        let keysym = xkb::keysym_from_name(
            format!("U{:X}", "å".chars().next().unwrap() as u32).as_str(),
            xkb::KEYSYM_NO_FLAGS,
        );
        let keysym = xkb::keysym_to_utf8(keysym);
        assert_eq!(keysym, "å\0");
    }
    
    #[test]
    fn test_key_unicode() {
        assert_eq!(
            create_symbol(
                &hashmap!{
                    ".".into() => ButtonMeta {
                        icon: None,
                        keysym: None,
                        action: None,
                        label: Some("test".into()),
                        outline: None,
                    }
                },
                ".",
                Vec::new()
            ),
            ::symbol::Symbol {
                action: ::symbol::Action::Submit {
                    text: None,
                    keys: vec!(::symbol::KeySym("U002E".into())),
                },
            }
        );
    }
}
