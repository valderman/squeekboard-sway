#include <gdk/gdk.h>
#include <xkbcommon/xkbcommon.h>

gboolean
squeek_keymap_get_entries_for_keyval (struct xkb_keymap     *xkb_keymap,
                                      guint          keyval,
                                      GdkKeymapKey **keys,
                                      guint          *n_keys);

static const char *keymap_header = "xkb_keymap {\n\
\n";

static const char *keymap_keycodes_header = "\
    xkb_keycodes \"squeekboard\" {\n\n\
        minimum = 8;\n\
        maximum = 255;\n\
\n";

static const char *keymap_symbols_header = "\
    xkb_symbols \"squeekboard\" {\n\
\n\
        name[Group1] = \"Letters\";\n\
        name[Group2] = \"Numbers/Symbols\";\n\
\n";

static const char *keymap_footer = "\
    xkb_types \"squeekboard\" {\n\
\n\
	type \"TWO_LEVEL\" {\n\
            modifiers = Shift;\n\
            map[Shift] = Level2;\n\
            level_name[Level1] = \"Base\";\n\
            level_name[Level2] = \"Shift\";\n\
	};\n\
    };\n\
\n\
    xkb_compatibility \"squeekboard\" {\n\
    };\n\
};";
