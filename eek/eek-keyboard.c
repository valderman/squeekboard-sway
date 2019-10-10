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
#include "eekboard/eekboard-context-service.h"
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

void
eek_keyboard_set_key_locked (LevelKeyboard    *keyboard,
                            struct squeek_key *key)
{
    EekModifierKey *modifier_key = g_slice_new (EekModifierKey);
    modifier_key->modifiers = 0;
    modifier_key->key = key;
    keyboard->locked_keys =
            g_list_prepend (keyboard->locked_keys, modifier_key);
}

/// Unlock all locked keys.
/// All locked keys will unlock at the next keypress (should be called "stuck")
/// Returns the number of handled keys
/// TODO: may need to check key type in order to chain locks
/// before pressing an "emitting" key
static int unlock_keys(LevelKeyboard *keyboard) {
    int handled = 0;
    for (GList *head = keyboard->locked_keys; head; ) {
        EekModifierKey *modifier_key = head->data;
        GList *next = g_list_next (head);
        keyboard->locked_keys =
                g_list_remove_link (keyboard->locked_keys, head);
        //squeek_key_set_locked(squeek_button_get_key(modifier_key->button), false);

        squeek_layout_set_state_from_press(keyboard->layout, keyboard, modifier_key->key);
        g_list_free1 (head);
        head = next;
        handled++;
    }
    return handled;
}

static void
set_level_from_press (LevelKeyboard *keyboard, struct squeek_key *key)
{
    // If the currently locked key was already handled in the unlock phase,
    // then skip
    if (unlock_keys(keyboard) == 0) {
        squeek_layout_set_state_from_press(keyboard->layout, keyboard, key);
    }
}

void eek_keyboard_press_key(LevelKeyboard *keyboard, struct squeek_key *key, guint32 timestamp) {
    keyboard->pressed_keys = g_list_prepend (keyboard->pressed_keys, key);
    squeek_key_press(key, keyboard->manager->virtual_keyboard, KEY_PRESS, timestamp);
}

void eek_keyboard_release_key(LevelKeyboard *keyboard,
                              struct squeek_key *key,
                              guint32 timestamp) {
    for (GList *head = keyboard->pressed_keys; head; head = g_list_next (head)) {
        if (squeek_key_equal(head->data, key)) {
            keyboard->pressed_keys = g_list_remove_link (keyboard->pressed_keys, head);
            g_list_free1 (head);
            break;
        }
    }

    set_level_from_press (keyboard, key);
    squeek_key_press(key, keyboard->manager->virtual_keyboard, KEY_RELEASE, timestamp);
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
