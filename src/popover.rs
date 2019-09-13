/*! The layout chooser popover */

use gio;
use gtk;
use ::layout::c::EekGtkKeyboard;

use gio::ActionExt;
use gio::ActionMapExt;
use gio::SettingsExt;
use glib::translate::FromGlibPtrNone;
use glib::variant::ToVariant;
use gtk::PopoverExt;
use gtk::WidgetExt;
use std::io::Write;

mod variants {
    use glib;
    use glib_sys;

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
}

fn make_menu_builder(inputs: Vec<&str>) -> gtk::Builder {
    let mut xml: Vec<u8> = Vec::new();
    writeln!(
        xml,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <menu id=\"app-menu\">
    <section>"
    ).unwrap();
    for input in inputs {
        writeln!(
            xml,
            "
        <item>
            <attribute name=\"label\" translatable=\"yes\">{}</attribute>
            <attribute name=\"action\">layout</attribute>
            <attribute name=\"target\">{0}</attribute>
        </item>",
            input,
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
    let inputs = variants::get_tuples(inputs).into_iter();
    for (index, (ikind, iname)) in inputs.enumerate() {
        if (&ikind, &iname) == (&kind, &name) {
            settings.set_uint("current", index as u32);
        }
    }
    settings.apply();
}

pub fn show(window: EekGtkKeyboard, position: ::layout::c::Bounds) {
    unsafe { gtk::set_initialized() };
    let window = unsafe { gtk::Widget::from_glib_none(window.0) };
    
    let settings = gio::Settings::new("org.gnome.desktop.input-sources");
    let inputs = settings.get_value("sources").unwrap();
    let current = settings.get_uint("current") as usize;
    let inputs = variants::get_tuples(inputs);
    
    let input_names: Vec<&str> = inputs.iter()
        .map(|(_kind, name)| name.as_str())
        .collect();

    let builder = make_menu_builder(input_names.clone());
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

    let initial_state = input_names[current].to_variant();

    let layout_action = gio::SimpleAction::new_stateful(
        "layout",
        Some(initial_state.type_()),
        &initial_state,
    );

    let action_group = gio::SimpleActionGroup::new();
    action_group.add_action(&layout_action);

    menu.insert_action_group("popup", Some(&action_group));
    menu.bind_model(Some(&model), Some("popup"));

    menu.connect_closed(move |_menu| {
        let state = match layout_action.get_state() {
            Some(v) => {
                let s = v.get::<String>().or_else(|| {
                    eprintln!("Variant is not string: {:?}", v);
                    None
                });
                // FIXME: the `get_state` docs call for unrefing,
                // but the function is nowhere to be found
                // glib::Variant::unref(v);
                s
            },
            None => {
                eprintln!("No variant selected");
                None
            },
        };
        set_layout("xkb".into(), state.unwrap_or("us".into()));
    });

    menu.popup();
}
