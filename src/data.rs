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
use ::util::hash_map_map;
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

        let layout = load_layout_with_fallback(name);
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

#[derive(Debug, Clone, PartialEq)]
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

/// Lists possible sources, with 0 as the most preferred one
fn list_layout_sources(
    name: &str,
    keyboards_path: Option<PathBuf>
) -> Vec<DataSource> {
    let mut ret = Vec::new();
    if let Some(path) = keyboards_path.clone() {
        ret.push(DataSource::File(path.join(name).with_extension("yaml")))
    }
    
    ret.push(DataSource::Resource(name.to_owned()));

    if let Some(path) = keyboards_path.clone() {
        ret.push(DataSource::File(
            path.join(FALLBACK_LAYOUT_NAME).with_extension("yaml")
        ))
    }
    
    ret.push(DataSource::Resource(FALLBACK_LAYOUT_NAME.to_owned()));
    ret
}

fn load_layout(source: DataSource) -> Result<::layout::Layout, LoadError> {
    match source {
        DataSource::File(path) => {
            Layout::from_file(path.clone())
                .map_err(LoadError::BadData)
                .and_then(|layout|
                    layout.build().map_err(LoadError::BadKeyMap)
                )
        },
        DataSource::Resource(name) => {
            Layout::from_resource(&name)
                .and_then(|layout|
                    layout.build().map_err(LoadError::BadKeyMap)
                )
        },
    }
}

fn load_layout_with_fallback(
    name: &str
) -> ::layout::Layout {
    let path = env::var_os("SQUEEKBOARD_KEYBOARDSDIR")
        .map(PathBuf::from)
        .or_else(|| xdg::data_path("squeekboard/keyboards"));
    
    for source in list_layout_sources(name, path) {
        let layout = load_layout(source.clone());
        match layout {
            Err(e) => match (e, source) {
                (
                    LoadError::BadData(Error::Missing(e)),
                    DataSource::File(file)
                ) => eprintln!( // TODO: print in debug logging level
                    "Tried file {:?}, but it's missing: {}",
                    file, e
                ),
                (e, source) => eprintln!(
                    "Failed to load layout from {}: {}, skipping",
                    source, e
                ),
            },
            Ok(layout) => return layout,
        }
    }

    panic!("No useful layout found!");
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

/// Errors encountered loading the layout into yaml
#[derive(Debug)]
pub enum Error {
    Yaml(serde_yaml::Error),
    Io(io::Error),
    /// The file was missing.
    /// It's distinct from Io in order to make it matchable
    /// without calling io::Error::kind()
    Missing(io::Error),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Yaml(e) => write!(f, "YAML: {}", e),
            Error::Io(e) => write!(f, "IO: {}", e),
            Error::Missing(e) => write!(f, "Missing: {}", e),
        }
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Self {
        let kind = e.kind();
        match kind {
            io::ErrorKind::NotFound => Error::Missing(e),
            _ => Error::Io(e),
        }
    }
}

impl Layout {
    pub fn from_resource(name: &str) -> Result<Layout, LoadError> {
        let data = resources::get_keyboard(name)
                    .ok_or(LoadError::MissingResource)?;
        serde_yaml::from_str(data)
                    .map_err(LoadError::BadResource)
    }

    fn from_file(path: PathBuf) -> Result<Layout, Error> {
        let infile = BufReader::new(
            fs::OpenOptions::new()
                .read(true)
                .open(&path)?
        );
        serde_yaml::from_reader(infile).map_err(Error::Yaml)
    }

    pub fn build(self) -> Result<::layout::Layout, FormattingError> {
        let button_names = self.views.values()
            .flat_map(|rows| {
                rows.iter()
                    .flat_map(|row| row.split_ascii_whitespace())
            });
        
        let button_names: HashSet<&str>
            = HashSet::from_iter(button_names);

        let button_actions: Vec<(&str, ::action::Action)>
            = button_names.iter().map(|name| {(
                *name,
                create_action(
                    &self.buttons,
                    name,
                    self.views.keys().collect()
                )
            )}).collect();

        let keymap: HashMap<String, u32> = generate_keycodes(
            button_actions.iter()
                .filter_map(|(_name, action)| {
                    match action {
                        ::action::Action::Submit {
                            text: _, keys,
                        } => Some(keys),
                        _ => None,
                    }
                })
                .flatten()
                .map(|named_keysym| named_keysym.0.as_str())
        );

        let button_states = button_actions.into_iter().map(|(name, action)| {
            let keycodes = match &action {
                ::action::Action::Submit { text: _, keys } => {
                    keys.iter().map(|named_keycode| {
                        *keymap.get(named_keycode.0.as_str())
                            .expect(
                                format!(
                                    "keycode {} in key {} missing from keymap",
                                    named_keycode.0,
                                    name
                                ).as_str()
                            )
                    }).collect()
                },
                _ => Vec::new(),
            };
            (
                name.into(),
                KeyState {
                    pressed: false,
                    locked: false,
                    keycodes,
                    action,
                }
            )
        });

        let button_states
            = HashMap::<String, KeyState>::from_iter(
                button_states
            );

        // TODO: generate from symbols
        let keymap_str = generate_keymap(&button_states)?;

        let button_states_cache = hash_map_map(
            button_states,
            |name, state| {(
                name,
                Rc::new(RefCell::new(state))
            )}
        );

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
                                    button_states_cache.get(name.into())
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

fn create_action(
    button_info: &HashMap<String, ButtonMeta>,
    name: &str,
    view_names: Vec<&String>,
) -> ::action::Action {
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
    
    let keysyms = match &symbol_meta.action {
        // Non-submit action
        Some(_) => Vec::new(),
        // Submit action
        None => match &symbol_meta.keysym {
            // Keysym given explicitly
            Some(keysym) => vec!(match keysym_valid(keysym.as_str()) {
                true => keysym.clone(),
                false => {
                    eprintln!("Keysym name invalid: {}", keysym);
                    "space".into() // placeholder
                },
            }),
            // Keysyms left open to derive
            // TODO: when button name is meant diretly as xkb keysym name,
            // mark it so, e.g. with a "#"
            None => match keysym_valid(name) {
                // Button name is actually a valid xkb name
                true => vec!(String::from(name)),
                // Button name is not a valid xkb name,
                // so assume it's a literal string to be submitted
                false => {
                    if name.chars().count() == 0 {
                        // A name read from yaml with no valid Unicode.
                        // Highly improbable, but let's be safe.
                        eprintln!("Key {} doesn't have any characters", name);
                        vec!("space".into()) // placeholder
                    } else {
                        name.chars().map(|codepoint| {
                            let codepoint_string = codepoint.to_string();
                            match keysym_valid(codepoint_string.as_str()) {
                                true => codepoint_string,
                                false => format!("U{:04X}", codepoint as u32),
                            }
                        }).collect()
                    }
                },
            },
        },
    };
    
    match &symbol_meta.action {
        Some(Action::SetView(view_name)) => ::action::Action::SetLevel(
            filter_view_name(name, view_name.clone(), &view_names)
        ),
        Some(Action::Locking {
            lock_view, unlock_view
        }) => ::action::Action::LockLevel {
            lock: filter_view_name(name, lock_view.clone(), &view_names),
            unlock: filter_view_name(
                name,
                unlock_view.clone(),
                &view_names
            ),
        },
        Some(Action::ShowPrefs) => ::action::Action::Submit {
            text: None,
            keys: Vec::new(),
        },
        None => ::action::Action::Submit {
            text: None,
            keys: keysyms.into_iter().map(::action::KeySym).collect(),
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
        outline_name: CString::new(outline_name).expect("Bad outline"),
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
            Layout::from_file(PathBuf::from("tests/layout.yaml")).unwrap(),
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
        let out = Layout::from_file(PathBuf::from("tests/layout2.yaml"));
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
        let out = Layout::from_file(PathBuf::from("tests/layout3.yaml"));
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
        let out = Layout::from_file(PathBuf::from("tests/layout_key1.yaml"))
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
        let out = Layout::from_file(PathBuf::from("tests/layout_key2.yaml"))
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

    /// Test multiple codepoints
    #[test]
    fn test_layout_unicode_multi() {
        let out = Layout::from_file(PathBuf::from("tests/layout_key3.yaml"))
            .unwrap()
            .build()
            .unwrap();
        assert_eq!(
            out.views["base"]
                .rows[0]
                .buttons[0]
                .state.borrow()
                .keycodes.len(),
            2
        );
    }

    #[test]
    fn parsing_fallback() {
        assert!(Layout::from_resource(FALLBACK_LAYOUT_NAME)
            .and_then(|layout| layout.build().map_err(LoadError::BadKeyMap))
            .is_ok()
        );
    }
    
    /// First fallback should be to builtin, not to FALLBACK_LAYOUT_NAME
    #[test]
    fn fallbacks_order() {
        let sources = list_layout_sources("nb", None);
        
        assert_eq!(
            sources,
            vec!(
                DataSource::Resource("nb".into()),
                DataSource::Resource(FALLBACK_LAYOUT_NAME.into()),
            )
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
            create_action(
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
            ::action::Action::Submit {
                text: None,
                keys: vec!(::action::KeySym("U002E".into())),
            }
        );
    }
}
