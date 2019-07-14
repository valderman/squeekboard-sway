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
#include "eekboard/keymap.h"

#include <gdk/gdk.h>

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


/* The following functions for keyboard mapping change are direct
   translation of the code in Caribou (in libcaribou/xadapter.vala):

   - get_replaced_keycode (Caribou: get_reserved_keycode)
   - replace_keycode
   - get_keycode_from_gdk_keymap (Caribou: best_keycode_keyval_match)
*/

/* Find an unused keycode where a keysym can be assigned. Restricted to Level 1 */
static guint
get_replaced_keycode (SeatEmitter *client)
{
    guint keycode;
return 0; // FIXME: no xkb allocated yet
    for (keycode = client->xkb->max_key_code;
         keycode >= client->xkb->min_key_code;
         --keycode) {
        guint offset = client->xkb->map->key_sym_map[keycode].offset;
        if (client->xkb->map->key_sym_map[keycode].kt_index[0] == XkbOneLevelIndex &&
            client->xkb->map->syms[offset] != NoSymbol) {
            return keycode;
        }
    }

    return 0;
}

/* Replace keysym assigned to KEYCODE to KEYSYM.  Both args are used
   as in-out.  If KEYCODE points to 0, this function picks a keycode
   from the current map and replace the associated keysym to KEYSYM.
   In that case, the replaced keycode is stored in KEYCODE and the old
   keysym is stored in KEYSYM.  If otherwise (KEYCODE points to
   non-zero keycode), it simply changes the current map with the
   specified KEYCODE and KEYSYM. */
static gboolean
replace_keycode (SeatEmitter *emitter,
                 guint   keycode,
                 guint  *keysym)
{
/*  GdkDisplay *display = gdk_display_get_default ();
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
    guint old_keysym;
    int keysyms_per_keycode;
    KeySym *syms;
*/
return TRUE; // FIXME: no xkb allocated at the moment, pretending all is fine
    g_return_val_if_fail (emitter->xkb->min_key_code <= keycode &&
                          keycode <= emitter->xkb->max_key_code,
                          FALSE);
    g_return_val_if_fail (keysym != NULL, FALSE);
/*
 * Update keyboard mapping. Wayland receives keyboard mapping as a string, so XChangeKeyboardMapping needs to translate from the symbol tbale t the string. TODO.
 *
    syms = XGetKeyboardMapping (xdisplay, keycode, 1, &keysyms_per_keycode);
    old_keysym = syms[0];
    syms[0] = *keysym;
    XChangeKeyboardMapping (xdisplay, keycode, 1, syms, 1);
    XSync (xdisplay, False);
    XFree (syms);
    *keysym = old_keysym;
*/
    return TRUE;
}

static gboolean
get_keycode_from_gdk_keymap (SeatEmitter *emitter,
                             guint           keysym,
                             guint          *keycode,
                             guint          *modifiers)
{
    GdkKeymapKey *keys, *best_match = NULL;
    guint n_keys, i;

    if (!squeek_keymap_get_entries_for_keyval (emitter->keymap, keysym, &keys, &n_keys))
        return FALSE;

    for (i = 0; i < n_keys; i++)
        if ((guint)keys[i].group == emitter->group)
            best_match = &keys[i];

    if (!best_match) {
        g_free (keys);
        return FALSE;
    }

    *keycode = best_match->keycode;
    *modifiers = best_match->level == 1 ? EEK_SHIFT_MASK : 0;

    g_free (keys);
    return TRUE;
}

int send_virtual_keyboard_key(
    struct zwp_virtual_keyboard_v1 *keyboard,
    unsigned int keycode,
    unsigned is_press,
    uint32_t timestamp
) {
    g_debug("send_virtual_keyboard_key: %u", keycode);
    zwp_virtual_keyboard_v1_key(keyboard, timestamp, keycode, (unsigned)is_press);
    return 0;
}

static void
send_fake_modifiers_events (SeatEmitter         *emitter,
                            EekModifierType      modifiers,
                            uint32_t             timestamp)
{
    (void)timestamp;

    uint32_t proto_modifiers = 0;
    if (modifiers & EEK_SHIFT_MASK) {
        proto_modifiers |= 1<<MOD_IDX_SHIFT;
    }
    if (modifiers & EEK_CONTROL_MASK) {
        proto_modifiers |= 1<<MOD_IDX_CTRL;
    }
    if (modifiers & EEK_MOD1_MASK) {
        proto_modifiers |= 1<<MOD_IDX_ALT;
    }
    g_debug("send_fake_modifiers_events: %u", proto_modifiers);
    zwp_virtual_keyboard_v1_modifiers(emitter->virtual_keyboard, proto_modifiers, 0, 0, emitter->group);
}

static void
send_fake_key_event (SeatEmitter *emitter,
                     guint    xkeysym,
                     guint    keyboard_modifiers,
                     gboolean pressed,
                     uint32_t timestamp)
{
    EekModifierType modifiers;
    guint old_keysym = xkeysym;

    g_return_if_fail (xkeysym > 0);
    g_debug("send_fake_key_event: %i %i", xkeysym, keyboard_modifiers);

    guint keycode;
    if (!get_keycode_from_gdk_keymap (emitter, xkeysym, &keycode, &modifiers)) {
        keycode = get_replaced_keycode (emitter);
        if (keycode == 0) {
            g_warning ("no available keycode to replace");
            return;
        }

        if (!replace_keycode (emitter, keycode, &old_keysym)) {
            g_warning ("failed to lookup X keysym %X", xkeysym);
            return;
        }
    }
    /* Clear level shift modifiers */
    keyboard_modifiers &= (unsigned)~EEK_SHIFT_MASK;
    keyboard_modifiers &= (unsigned)~EEK_LOCK_MASK;
    /* FIXME: may need to remap ISO_Level3_Shift and NumLock */

    modifiers |= keyboard_modifiers;

    send_fake_modifiers_events (emitter, modifiers, timestamp);

    // There's something magical about subtracting/adding 8 to keycodes for some reason
    send_virtual_keyboard_key (emitter->virtual_keyboard, keycode - 8, (unsigned)pressed, timestamp);
    send_fake_modifiers_events (emitter, modifiers, timestamp);

    if (old_keysym != xkeysym)
        replace_keycode (emitter, keycode, &old_keysym);
}

static void
send_fake_key_events (SeatEmitter *emitter,
                      EekSymbol *symbol,
                      EekModifierType      keyboard_modifiers,
                      gboolean   pressed,
                      uint32_t   timestamp)
{
    /* Ignore modifier keys */
    if (eek_symbol_is_modifier (symbol))
        return;

    g_debug("symbol: %s", eek_symbol_get_name(symbol));
    /* If symbol is a text, convert chars in it to keysym */
    if (EEK_IS_TEXT(symbol)) {
        const gchar *utf8 = eek_text_get_text (EEK_TEXT(symbol));
        printf("Attempting to send text %s\n", utf8);
        /* FIXME:
        glong items_written;
        gunichar *ucs4 = g_utf8_to_ucs4_fast (utf8, -1, &items_written);
        gint i;

        for (i = 0; i < items_written; i++) {
            guint xkeysym;
            EekKeysym *keysym;
            gchar *name;

            name = g_strdup_printf ("U%04X", ucs4[i]);
            xkeysym = XStringToKeysym (name); // TODO: use xkb_get_keysym_from_name
            g_free (name);

            keysym = eek_keysym_new (xkeysym);
            send_fake_key_events (client,
                                  EEK_SYMBOL(keysym),
                                  keyboard_modifiers);
        }
        g_free (ucs4);
        */
        return;
    }

    if (EEK_IS_KEYSYM(symbol)) {
        g_debug("keysym: %u", eek_keysym_get_xkeysym(EEK_KEYSYM(symbol)));
        guint xkeysym = eek_keysym_get_xkeysym (EEK_KEYSYM(symbol));
        send_fake_key_event (emitter, xkeysym, keyboard_modifiers, pressed, timestamp);
    }
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
    guint level = eek_element_get_level(EEK_ELEMENT(keyboard));
    uint32_t group = (level / 2);

    g_debug("send_fake_key: %u %u", keycode, keyboard_modifiers);
    g_debug("level: %u", level);
    g_debug("group: %u", group);

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
    g_debug("symbol: %s", eek_symbol_get_name(symbol));
    g_debug("keycode: %u", keycode);
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
