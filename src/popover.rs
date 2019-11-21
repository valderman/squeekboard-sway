/*! The layout chooser popover */

use gio;
use gtk;
use ::layout::c::EekGtkKeyboard;
use ::locale::compare_current_locale;
use ::locale_config::system_locale;
use ::resources;

use gio::ActionMapExt;
use gio::SettingsExt;
use gio::SimpleActionExt;
use glib::translate::FromGlibPtrNone;
use glib::variant::ToVariant;
use gtk::PopoverExt;
use gtk::WidgetExt;
use std::io::Write;

mod variants {
    use glib;
    use glib::Variant;
    use glib_sys;
    use std::os::raw::c_char;

    use glib::ToVariant;
    use glib::translate::FromGlibPtrFull;
    use glib::translate::ToGlibPtr;

    /// Unpacks tuple & array variants
    fn get_items(items: glib::Variant) -> Vec<glib::Variant> {
        let variant_naked = items.to_glib_none().0;
        let count = unsafe { glib_sys::g_variant_n_children(variant_naked) };
        (0..count).map(|index| 
            unsafe {
                glib::Variant::from_glib_full(
                    glib_sys::g_variant_get_child_value(variant_naked, index)
                )
            }
        ).collect()
    }

    /// Unpacks "a(ss)" variants
    pub fn get_tuples(items: glib::Variant) -> Vec<(String, String)> {
        get_items(items)
            .into_iter()
            .map(get_items)
            .map(|v| {
                (
                    v[0].get::<String>().unwrap(),
                    v[1].get::<String>().unwrap(),
                )
            })
            .collect()
    }

    /// "a(ss)" variant
    /// Rust doesn't allow implementing existing traits for existing types
    pub struct ArrayPairString(pub Vec<(String, String)>);
    
    impl ToVariant for ArrayPairString {
        fn to_variant(&self) -> Variant {
            let tspec = "a(ss)".to_glib_none();
            let builder = unsafe {
                let vtype = glib_sys::g_variant_type_checked_(tspec.0);
                glib_sys::g_variant_builder_new(vtype)
            };
            let ispec = "(ss)".to_glib_none();
            for (a, b) in &self.0 {
                let a = a.to_glib_none();
                let b = b.to_glib_none();
                // string pointers are weak references
                // and will get silently invalidated
                // as soon as the source is out of scope
                {
                    let a: *const c_char = a.0;
                    let b: *const c_char = b.0;
                    unsafe {
                        glib_sys::g_variant_builder_add(
                            builder,
                            ispec.0,
                            a, b
                        );
                    }
                }
            }
            unsafe {
                let ret = glib_sys::g_variant_builder_end(builder);
                glib_sys::g_variant_builder_unref(builder);
                glib::Variant::from_glib_full(ret)
            }
        }
    }
}

fn make_menu_builder(inputs: Vec<(&str, &str)>) -> gtk::Builder {
    let mut xml: Vec<u8> = Vec::new();
    writeln!(
        xml,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <menu id=\"app-menu\">
    <section>"
    ).unwrap();
    for (input_name, human_name) in inputs {
        writeln!(
            xml,
            "
        <item>
            <attribute name=\"label\" translatable=\"yes\">{}</attribute>
            <attribute name=\"action\">layout</attribute>
            <attribute name=\"target\">{}</attribute>
        </item>",
            human_name,
            input_name,
        ).unwrap();
    }
    writeln!(
        xml,
        "
    </section>
  </menu>
</interface>"
    ).unwrap();
    gtk::Builder::new_from_string(
        &String::from_utf8(xml).expect("Bad menu definition")
    )
}

fn set_layout(kind: String, name: String) {
    let settings = gio::Settings::new("org.gnome.desktop.input-sources");
    let inputs = settings.get_value("sources").unwrap();
    let current = (kind.clone(), name.clone());
    let inputs = variants::get_tuples(inputs).into_iter()
        .filter(|t| t != &current);
    let inputs = vec![(kind, name)].into_iter()
        .chain(inputs).collect();
    settings.set_value(
        "sources",
        &variants::ArrayPairString(inputs).to_variant()
    );
    settings.apply();
}

pub fn show(window: EekGtkKeyboard, position: ::layout::c::Bounds) {
    unsafe { gtk::set_initialized() };
    let window = unsafe { gtk::Widget::from_glib_none(window.0) };

    let settings = gio::Settings::new("org.gnome.desktop.input-sources");
    let inputs = settings.get_value("sources").unwrap();
    let inputs = variants::get_tuples(inputs);
    
    let input_names: Vec<&str> = inputs.iter()
        .map(|(_kind, name)| name.as_str())
        .collect();

    let translations = system_locale()
        .map(|locale|
            locale.tags_for("messages")
                .next().unwrap() // guaranteed to exist
                .as_ref()
                .to_owned()
        )
        .and_then(|lang| resources::get_layout_names(lang.as_str()));

    // sorted collection of human and machine names
    let mut human_names: Vec<(&str, &str)> = match translations {
        Some(translations) => {
            input_names.iter()
                .map(|name| (*name, *translations.get(name).unwrap_or(name)))
                .collect()
        },
        // display bare codes
        None => {
            input_names.iter()
                .map(|n| (*n, *n)) // turns &&str into &str
                .collect()
        }
    };

    human_names.sort_unstable_by(|(_, human_label_a), (_, human_label_b)| {
        compare_current_locale(human_label_a, human_label_b)
    });

    let builder = make_menu_builder(human_names);
    // Much more debuggable to populate the model & menu
    // from a string representation
    // than add items imperatively
    let model: gio::MenuModel = builder.get_object("app-menu").unwrap();

    let menu = gtk::Popover::new_from_model(Some(&window), &model);
    menu.set_pointing_to(&gtk::Rectangle {
        x: position.x.ceil() as i32,
        y: position.y.ceil() as i32,
        width: position.width.floor() as i32,
        height: position.width.floor() as i32,
    });

    if let Some(current_name) = input_names.get(0) {
        let current_name = current_name.to_variant();

        let layout_action = gio::SimpleAction::new_stateful(
            "layout",
            Some(current_name.type_()),
            &current_name,
        );

        layout_action.connect_change_state(|_action, state| {
            match state {
                Some(v) => {
                    v.get::<String>()
                        .or_else(|| {
                            eprintln!("Variant is not string: {:?}", v);
                            None
                        })
                        .map(|state| set_layout("xkb".into(), state));
                },
                None => eprintln!("No variant selected"),
            };
        });

        let action_group = gio::SimpleActionGroup::new();
        action_group.add_action(&layout_action);

        menu.insert_action_group("popup", Some(&action_group));
    };

    menu.bind_model(Some(&model), Some("popup"));
    menu.popup();
}
