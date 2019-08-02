/*
 * Copyright (C) 2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2011 Red Hat, Inc.
 * Copyright (C) 2019 Purism, SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file is responsible for managing keycode data and emitting keycodes. */

#include "eekboard/key-emitter.h"

#include <gdk/gdk.h>
#include <X11/XKBlib.h>

#include "eekboard/eekboard-context-service.h"

// TODO: decide whether it's this struct that carries the keyboard around in key-emitter or if the whole manager should be dragged around
// if this is the carrier, then it should be made part of the manager
// hint: check which fields need to be persisted between keypresses; which between keyboards
typedef struct {
    struct zwp_virtual_keyboard_v1 *virtual_keyboard; // unowned copy
    struct xkb_keymap *keymap; // unowned copy
    XkbDescRec *xkb;
    guint modifier_keycodes[8];
    guint modifier_indices[MOD_IDX_LAST];
    guint group;
} SeatEmitter;


int send_virtual_keyboard_key(
    struct zwp_virtual_keyboard_v1 *keyboard,
    unsigned int keycode,
    unsigned is_press,
    uint32_t timestamp
) {
    zwp_virtual_keyboard_v1_key(keyboard, timestamp, keycode, (unsigned)is_press);
    return 0;
}

/* Finds the first key code for each modifier and saves it in modifier_keycodes */
static void
update_modifier_info (SeatEmitter *client)
{
    client->modifier_indices[MOD_IDX_SHIFT] = xkb_keymap_mod_get_index(client->keymap, XKB_MOD_NAME_SHIFT);
    client->modifier_indices[MOD_IDX_CAPS] = xkb_keymap_mod_get_index(client->keymap, XKB_MOD_NAME_CAPS);
    client->modifier_indices[MOD_IDX_CTRL] = xkb_keymap_mod_get_index(client->keymap, XKB_MOD_NAME_CTRL);
    client->modifier_indices[MOD_IDX_ALT] = xkb_keymap_mod_get_index(client->keymap, XKB_MOD_NAME_ALT);
    client->modifier_indices[MOD_IDX_NUM] = xkb_keymap_mod_get_index(client->keymap, XKB_MOD_NAME_NUM);
    client->modifier_indices[MOD_IDX_MOD3] = xkb_keymap_mod_get_index(client->keymap, "Mod3");
    client->modifier_indices[MOD_IDX_LOGO] = xkb_keymap_mod_get_index(client->keymap, XKB_MOD_NAME_LOGO);
    client->modifier_indices[MOD_IDX_ALTGR] = xkb_keymap_mod_get_index(client->keymap, "Mod5");
    client->modifier_indices[MOD_IDX_NUMLK] = xkb_keymap_mod_get_index(client->keymap, "NumLock");
    client->modifier_indices[MOD_IDX_ALSO_ALT] = xkb_keymap_mod_get_index(client->keymap, "Alt");
    client->modifier_indices[MOD_IDX_LVL3] = xkb_keymap_mod_get_index(client->keymap, "LevelThree");
    client->modifier_indices[MOD_IDX_LALT] = xkb_keymap_mod_get_index(client->keymap, "LAlt");
    client->modifier_indices[MOD_IDX_RALT] = xkb_keymap_mod_get_index(client->keymap, "RAlt");
    client->modifier_indices[MOD_IDX_RCONTROL] = xkb_keymap_mod_get_index(client->keymap, "RControl");
    client->modifier_indices[MOD_IDX_LCONTROL] = xkb_keymap_mod_get_index(client->keymap, "LControl");
    client->modifier_indices[MOD_IDX_SCROLLLK] = xkb_keymap_mod_get_index(client->keymap, "ScrollLock");
    client->modifier_indices[MOD_IDX_LVL5] = xkb_keymap_mod_get_index(client->keymap, "LevelFive");
    client->modifier_indices[MOD_IDX_ALSO_ALTGR] = xkb_keymap_mod_get_index(client->keymap, "AltGr");
    client->modifier_indices[MOD_IDX_META] = xkb_keymap_mod_get_index(client->keymap, "Meta");
    client->modifier_indices[MOD_IDX_SUPER] = xkb_keymap_mod_get_index(client->keymap, "Super");
    client->modifier_indices[MOD_IDX_HYPER] = xkb_keymap_mod_get_index(client->keymap, "Hyper");

    /*
    for (xkb_mod_index_t i = 0;
         i < xkb_keymap_num_mods(client->keymap);
         i++) {
        g_log("squeek", G_LOG_LEVEL_DEBUG, "%s", xkb_keymap_mod_get_name(client->keymap, i));
    }*/
}

static void
send_fake_key (SeatEmitter *emitter,
               EekKeyboard *keyboard,
               guint    keycode,
               guint    keyboard_modifiers,
               gboolean pressed,
               uint32_t timestamp)
{
    uint32_t proto_modifiers = 0;
    guint level = keyboard->level;
    uint32_t group = (level / 2);

    if (keyboard_modifiers & EEK_SHIFT_MASK)
        proto_modifiers |= 1<<MOD_IDX_SHIFT;

    zwp_virtual_keyboard_v1_modifiers(emitter->virtual_keyboard, proto_modifiers, 0, 0, group);
    send_virtual_keyboard_key (emitter->virtual_keyboard, keycode - 8, (unsigned)pressed, timestamp);
    zwp_virtual_keyboard_v1_modifiers(emitter->virtual_keyboard, proto_modifiers, 0, 0, group);
}

void
emit_key_activated (EekboardContextService *manager,
                    EekKeyboard     *keyboard,
                    guint            keycode,
                    EekSymbol       *symbol,
                    EekModifierType  modifiers,
                    gboolean pressed,
                    uint32_t timestamp)
{
    /* FIXME: figure out how to deal with Client after key presses go through
    if (g_strcmp0 (eek_symbol_get_name (symbol), "cycle-keyboard") == 0) {
        client->keyboards_head = g_slist_next (client->keyboards_head);
        if (client->keyboards_head == NULL)
            client->keyboards_head = client->keyboards;
        eekboard_context_set_keyboard (client->context,
                                       GPOINTER_TO_UINT(client->keyboards_head->data),
                                       NULL);
        return;
    }

    if (g_strcmp0 (eek_symbol_get_name (symbol), "preferences") == 0) {
        gchar *argv[2];
        GError *error;

        argv[0] = g_build_filename (LIBEXECDIR, "eekboard-setup", NULL);
        argv[1] = NULL;

        error = NULL;
        if (!g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, NULL, &error)) {
            g_warning ("can't spawn %s: %s", argv[0], error->message);
            g_error_free (error);
        }
        g_free (argv[0]);
        return;
    }
*/
    SeatEmitter emitter = {0};
    emitter.virtual_keyboard = manager->virtual_keyboard;
    emitter.keymap = keyboard->keymap;
    update_modifier_info (&emitter);
    send_fake_key (&emitter, keyboard, keycode, modifiers, pressed, timestamp);
}
