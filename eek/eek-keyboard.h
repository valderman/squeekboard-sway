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

#if !defined(__EEK_H_INSIDE__) && !defined(EEK_COMPILATION)
#error "Only <eek/eek.h> can be included directly."
#endif

#ifndef EEK_KEYBOARD_H
#define EEK_KEYBOARD_H 1

#include <glib-object.h>
#include <xkbcommon/xkbcommon.h>
#include "eek-types.h"
#include "eek-layout.h"
#include "src/layout.h"

G_BEGIN_DECLS

struct _EekModifierKey {
    /*< public >*/
    EekModifierType modifiers;
    struct squeek_button *button;
};
typedef struct _EekModifierKey EekModifierKey;

/// Keyboard state holder
struct _LevelKeyboard {
    struct squeek_view *views[4];
    guint level;
    struct xkb_keymap *keymap;
    int keymap_fd; // keymap formatted as XKB string
    size_t keymap_len; // length of the data inside keymap_fd
    GArray *outline_array;

    GList *pressed_buttons; // struct squeek_button*
    GList *locked_buttons; // struct squeek_button*

    /* Map button names to button objects: */
    GHashTable *names;

    guint id; // as a key to layout choices

    EekboardContextService *manager; // unowned reference
};
typedef struct _LevelKeyboard LevelKeyboard;

struct squeek_button *eek_keyboard_find_button_by_name(LevelKeyboard *keyboard,
                                      const gchar        *name);

/// Represents the path to the button within a view
struct button_place {
    const struct squeek_row *row;
    const struct squeek_button *button;
};

EekOutline         *level_keyboard_get_outline
                                     (LevelKeyboard        *keyboard,
                                      guint               oref);
EekModifierKey     *eek_modifier_key_copy
                                     (EekModifierKey     *modkey);
void                eek_modifier_key_free
                                     (EekModifierKey      *modkey);

void eek_keyboard_press_button(LevelKeyboard *keyboard, struct squeek_button *button, guint32 timestamp);
void eek_keyboard_release_button(LevelKeyboard *keyboard, struct squeek_button *button, guint32 timestamp);

gchar *             eek_keyboard_get_keymap
                                     (LevelKeyboard *keyboard);

struct squeek_view *level_keyboard_current(LevelKeyboard *keyboard);
LevelKeyboard *level_keyboard_new(EekboardContextService *manager, struct squeek_view *views[], GHashTable *name_button_hash);
void level_keyboard_deinit(LevelKeyboard *self);
void level_keyboard_free(LevelKeyboard *self);

G_END_DECLS
#endif  /* EEK_KEYBOARD_H */
