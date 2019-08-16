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

#define EEK_TYPE_KEYBOARD (eek_keyboard_get_type())
#define EEK_KEYBOARD(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEK_TYPE_KEYBOARD, EekKeyboard))
#define EEK_KEYBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), EEK_TYPE_KEYBOARD, EekKeyboardClass))
#define EEK_IS_KEYBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEK_TYPE_KEYBOARD))
#define EEK_IS_KEYBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EEK_TYPE_KEYBOARD))
#define EEK_KEYBOARD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EEK_TYPE_KEYBOARD, EekKeyboardClass))

typedef struct _EekKeyboardClass EekKeyboardClass;
typedef struct _EekKeyboardPrivate EekKeyboardPrivate;

/**
 * EekKeyboard:
 *
 * Contains the graphical representation of the rows of buttons.
 */
struct _EekKeyboard
{
    /*< private >*/
    EekElement parent;

    EekKeyboardPrivate *priv;
    GPtrArray *rows; // struct squeek_row*
};

/**
 * EekKeyboardClass:
 * @create_section: virtual function for creating a section
 * @find_key_by_name: virtual function for finding a key in the
 * keyboard by name
 * @key_pressed: class handler for #EekKeyboard::key-pressed signal
 * @key_released: class handler for #EekKeyboard::key-released signal
 * @key_locked: class handler for #EekKeyboard::key-locked signal
 * @key_unlocked: class handler for #EekKeyboard::key-unlocked signal
 * @key_cancelled: class handler for #EekKeyboard::key-cancelled signal
 */
struct _EekKeyboardClass
{
    /*< private >*/
    EekElementClass parent_class;

    /* obsolete members moved to EekElement */
    gpointer set_symbol_index;
    gpointer get_symbol_index;

    /*< private >*/
    /* obsolete members moved to EekElement */
    gpointer symbol_index_changed;

    /*< private >*/
    /* padding */
    gpointer pdummy[21];
};

/**
 * EekModifierKey:
 * @modifiers: an #EekModifierType which @key triggers
 * @key: an #EekKey
 *
 * Entry which associates modifier mask to a key.  This is returned by
 * eek_keyboard_get_locked_keys().
 */
struct _EekModifierKey {
    /*< public >*/
    EekModifierType modifiers;
    struct squeek_button *button;
};
typedef struct _EekModifierKey EekModifierKey;

/// Keyboard state holder
struct _LevelKeyboard {
    EekKeyboard *views[4];
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

GType               eek_keyboard_get_type
                                     (void) G_GNUC_CONST;
void                eek_keyboard_get_size
                                     (EekKeyboard        *keyboard,
                                      gdouble            *width,
                                      gdouble            *height);
void                eek_keyboard_set_size
                                     (EekKeyboard        *keyboard,
                                      gdouble             width,
                                      gdouble             height);

struct squeek_row *eek_keyboard_get_row(EekKeyboard *keyboard,
                                      struct squeek_button *button);
struct squeek_button *eek_keyboard_find_button_by_name(LevelKeyboard *keyboard,
                                      const gchar        *name);

/// Represents the path to the button within a view
struct button_place {
    struct squeek_row *row;
    struct squeek_button *button;
};

struct button_place eek_keyboard_get_button_by_state(EekKeyboard *keyboard,
                                             struct squeek_key *key);

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

void eek_keyboard_foreach (EekKeyboard *keyboard,
                     GFunc      func,
                          gpointer   user_data);

EekKeyboard *level_keyboard_current(LevelKeyboard *keyboard);
LevelKeyboard *level_keyboard_new(EekboardContextService *manager, EekKeyboard *views[4], GHashTable *name_button_hash);
void level_keyboard_deinit(LevelKeyboard *self);
void level_keyboard_free(LevelKeyboard *self);

struct squeek_row *
eek_keyboard_real_create_row (EekKeyboard *self);


G_END_DECLS
#endif  /* EEK_KEYBOARD_H */
