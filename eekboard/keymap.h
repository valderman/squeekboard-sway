#include <gdk/gdk.h>
#include <xkbcommon/xkbcommon.h>

gboolean
squeek_keymap_get_entries_for_keyval (struct xkb_keymap     *xkb_keymap,
                                      guint          keyval,
                                      GdkKeymapKey **keys,
                                      guint          *n_keys);
