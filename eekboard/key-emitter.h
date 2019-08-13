#ifndef KEYEMITTER_H
#define KEYEMITTER_H

#include <inttypes.h>
#include <glib.h>

#include "eek/eek.h"

#include "virtual-keyboard-unstable-v1-client-protocol.h"

/// Indices obtained by xkb_keymap_mod_get_name
enum mod_indices {
    MOD_IDX_SHIFT,
    MOD_IDX_CAPS,
    MOD_IDX_CTRL,
    MOD_IDX_ALT,
    MOD_IDX_NUM,
    MOD_IDX_MOD3,
    MOD_IDX_LOGO,
    MOD_IDX_ALTGR,
    MOD_IDX_NUMLK, // Caution, not sure which is the right one
    MOD_IDX_ALSO_ALT, // Not sure why, alt emits the first alt on my setup
    MOD_IDX_LVL3,

    // Not sure if the next 4 are used at all
    MOD_IDX_LALT,
    MOD_IDX_RALT,
    MOD_IDX_RCONTROL,
    MOD_IDX_LCONTROL,

    MOD_IDX_SCROLLLK,
    MOD_IDX_LVL5,
    MOD_IDX_ALSO_ALTGR, // Not used on my layout
    MOD_IDX_META,
    MOD_IDX_SUPER,
    MOD_IDX_HYPER,

    MOD_IDX_LAST,
};

void
emit_key_activated (EekboardContextService *manager, LevelKeyboard *keyboard,
                    guint            keycode,
                    guint            modifiers,
                    gboolean pressed, uint32_t timestamp);
#endif // KEYEMITTER_H
