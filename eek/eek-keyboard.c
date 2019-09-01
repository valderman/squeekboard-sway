/*
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * SECTION:eek-keyboard
 * @short_description: Base class of a keyboard
 * @see_also: #EekSection
 *
 * The #EekKeyboardClass class represents a keyboard, which consists
 * of one or more sections of the #EekSectionClass class.
 */

#include "config.h"
#include <glib/gprintf.h>

#include "eek-enumtypes.h"
#include "eekboard/key-emitter.h"
#include "keymap.h"
#include "src/keyboard.h"

#include "eek-keyboard.h"

EekModifierKey *
eek_modifier_key_copy (EekModifierKey *modkey)
{
    return g_slice_dup (EekModifierKey, modkey);
}

void
eek_modifier_key_free (EekModifierKey *modkey)
{
    g_slice_free (EekModifierKey, modkey);
}

/// Updates the state of locked keys based on the key that was activated
/// FIXME: make independent of what the key are named,
/// and instead refer to the contained symbols
static guint
set_key_states (LevelKeyboard    *keyboard,
                struct squeek_button *button,
                guint new_level)
{
    // Keys locking rules hardcoded for the time being...
    const gchar *name = squeek_button_get_name(button);
    // Lock the shift whenever it's pressed on the baselevel
    // TODO: need to lock shift on the destination level
    if (g_strcmp0(name, "Shift_L") == 0 && squeek_layout_get_level(keyboard->layout) == 0) {
        EekModifierKey *modifier_key = g_slice_new (EekModifierKey);
        modifier_key->modifiers = 0;
        modifier_key->button = button;
        keyboard->locked_buttons =
            g_list_prepend (keyboard->locked_buttons, modifier_key);
        struct squeek_key *key = squeek_button_get_key(button);
        squeek_key_set_locked(key, true);
    }
    if (squeek_layout_get_level(keyboard->layout) == 1) {
        // Only shift is locked in this state, unlock on any key press
        for (GList *head = keyboard->locked_buttons; head; ) {
            EekModifierKey *modifier_key = head->data;
            GList *next = g_list_next (head);
            keyboard->locked_buttons =
                g_list_remove_link (keyboard->locked_buttons, head);
            squeek_key_set_locked(squeek_button_get_key(modifier_key->button), false);
            g_list_free1 (head);
            head = next;
        }
        return 0;
    }
    return new_level;
}

// FIXME: unhardcode, parse some user information as to which key triggers which view (level)
static void
set_level_from_press (LevelKeyboard *keyboard, struct squeek_button *button)
{
    /* The levels are: 0 Letters, 1 Upper case letters, 2 Numbers, 3 Symbols */
    guint level = squeek_layout_get_level(keyboard->layout);
    /* Handle non-emitting keys */
    if (button) {
        const gchar *name = squeek_button_get_name(button);
        if (g_strcmp0(name, "show_numbers") == 0) {
            level = 2;
        } else if (g_strcmp0(name, "show_letters") == 0) {
            level = 0;
        } else if (g_strcmp0(name, "show_symbols") == 0) {
            level = 3;
        } else if (g_strcmp0(name, "Shift_L") == 0) {
            level ^= 1;
        }
    }

    squeek_layout_set_level(keyboard->layout, set_key_states(keyboard, button, level));
}

void eek_keyboard_press_button(LevelKeyboard *keyboard, struct squeek_button *button, guint32 timestamp) {
    struct squeek_key *key = squeek_button_get_key(button);
    squeek_key_set_pressed(key, TRUE);
    keyboard->pressed_buttons = g_list_prepend (keyboard->pressed_buttons, button);

    // Only take action about setting level *after* the key has taken effect, i.e. on release
    //set_level_from_press (keyboard, key);

    // "Borrowed" from eek-context-service; doesn't influence the state but forwards the event

    guint keycode = squeek_key_get_keycode (key);

    emit_key_activated(keyboard->manager, keyboard, keycode, TRUE, timestamp);
}

void eek_keyboard_release_button(LevelKeyboard *keyboard,
                                 struct squeek_button *button,
                                 guint32 timestamp) {
    for (GList *head = keyboard->pressed_buttons; head; head = g_list_next (head)) {
        if (head->data == button) {
            keyboard->pressed_buttons = g_list_remove_link (keyboard->pressed_buttons, head);
            g_list_free1 (head);
            break;
        }
    }

    set_level_from_press (keyboard, button);

    // "Borrowed" from eek-context-service; doesn't influence the state but forwards the event

    guint keycode = squeek_key_get_keycode (squeek_button_get_key(button));

    emit_key_activated(keyboard->manager, keyboard, keycode, FALSE, timestamp);
}

void level_keyboard_deinit(LevelKeyboard *self) {
    squeek_layout_free(self->layout);
}

void level_keyboard_free(LevelKeyboard *self) {
    level_keyboard_deinit(self);
    g_free(self);
}

void level_keyboard_init(LevelKeyboard *self, struct squeek_layout *layout) {
    self->layout = layout;
}

LevelKeyboard *level_keyboard_new(EekboardContextService *manager, struct squeek_layout *layout) {
    LevelKeyboard *keyboard = g_new0(LevelKeyboard, 1);
    level_keyboard_init(keyboard, layout);
    keyboard->manager = manager;
    return keyboard;
}

struct squeek_view *level_keyboard_current(LevelKeyboard *keyboard)
{
    return squeek_layout_get_current_view(keyboard->layout);
}
