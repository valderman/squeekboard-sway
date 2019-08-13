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

#include "eek-section.h"
#include "eek-key.h"
#include "eek-enumtypes.h"
#include "eekboard/key-emitter.h"
#include "keymap.h"
#include "src/keyboard.h"
#include "src/symbol.h"

#include "eek-keyboard.h"

enum {
    PROP_0,
    PROP_LAST
};

enum {
    KEY_RELEASED,
    KEY_LOCKED,
    KEY_UNLOCKED,
    LAST_SIGNAL
};

enum {
    VIEW_LETTERS_LOWER,
    VIEW_LETTERS_UPPER,
    VIEW_NUMBERS,
    VIEW_SYMBOLS
};

#define EEK_KEYBOARD_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EEK_TYPE_KEYBOARD, EekKeyboardPrivate))

struct _EekKeyboardPrivate
{
    char dummy;    // won't run otherwise
};

G_DEFINE_TYPE_WITH_PRIVATE (EekKeyboard, eek_keyboard, EEK_TYPE_CONTAINER);

G_DEFINE_BOXED_TYPE(EekModifierKey, eek_modifier_key,
                    eek_modifier_key_copy, eek_modifier_key_free);

EekModifierKey *
eek_modifier_key_copy (EekModifierKey *modkey)
{
    return g_slice_dup (EekModifierKey, modkey);
}

void
eek_modifier_key_free (EekModifierKey *modkey)
{
    g_object_unref (modkey->key);
    g_slice_free (EekModifierKey, modkey);
}

EekSection *
eek_keyboard_real_create_section (EekKeyboard *self)
{
    EekSection *section;

    section = g_object_new (EEK_TYPE_SECTION, NULL);
    g_return_val_if_fail (section, NULL);

    EEK_CONTAINER_GET_CLASS(self)->add_child (EEK_CONTAINER(self),
                                              EEK_ELEMENT(section));
    return section;
}

static void
eek_keyboard_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_keyboard_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/// Updates the state of locked keys based on the key that was activated
/// FIXME: make independent of what the key are named,
/// and instead refer to the contained symbols
static guint
set_key_states (LevelKeyboard    *keyboard,
                        EekKey         *key,
                guint new_level)
{
    // Keys locking rules hardcoded for the time being...
    const gchar *name = eek_element_get_name(EEK_ELEMENT(key));
    // Lock the shift whenever it's pressed on the baselevel
    // TODO: need to lock shift on the destination level
    if (g_strcmp0(name, "Shift_L") == 0 && keyboard->level == 0) {
        EekModifierKey *modifier_key = g_slice_new (EekModifierKey);
        modifier_key->modifiers = 0;
        modifier_key->key = g_object_ref (key);
        keyboard->locked_keys =
            g_list_prepend (keyboard->locked_keys, modifier_key);
        eek_key_set_locked(modifier_key->key, true);
    }

    if (keyboard->level == 1) {
        // Only shift is locked in this state, unlock on any key press
        for (GList *head = keyboard->locked_keys; head; ) {
            EekModifierKey *modifier_key = head->data;
            GList *next = g_list_next (head);
            keyboard->locked_keys =
                g_list_remove_link (keyboard->locked_keys, head);
            eek_key_set_locked(modifier_key->key, false);
            g_list_free1 (head);
            head = next;
        }
        return 0;
    }
    return new_level;
}

// FIXME: unhardcode, parse some user information as to which key triggers which view (level)
static void
set_level_from_press (LevelKeyboard *keyboard, EekKey *key)
{
    /* The levels are: 0 Letters, 1 Upper case letters, 2 Numbers, 3 Symbols */
    guint level = keyboard->level;
    /* Handle non-emitting keys */
    if (key) {
        const gchar *name = eek_element_get_name(EEK_ELEMENT(key));
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

    keyboard->level = set_key_states(keyboard, key, level);

    eek_layout_update_layout(keyboard);
}

void eek_keyboard_press_key(LevelKeyboard *keyboard, EekKey *key, guint32 timestamp) {
    eek_key_set_pressed(key, TRUE);
    keyboard->pressed_keys = g_list_prepend (keyboard->pressed_keys, key);

    struct squeek_symbol *symbol = eek_key_get_symbol_at_index(
        key, 0
    );
    if (!symbol)
        return;

    // Only take action about setting level *after* the key has taken effect, i.e. on release
    //set_level_from_press (keyboard, key);

    // "Borrowed" from eek-context-service; doesn't influence the state but forwards the event

    guint keycode = eek_key_get_keycode (key);

    emit_key_activated(keyboard->manager, keyboard, keycode, TRUE, timestamp);
}

void eek_keyboard_release_key(LevelKeyboard *keyboard,
                               EekKey      *key,
                               guint32      timestamp) {
    for (GList *head = keyboard->pressed_keys; head; head = g_list_next (head)) {
        if (head->data == key) {
            keyboard->pressed_keys = g_list_remove_link (keyboard->pressed_keys, head);
            g_list_free1 (head);
            break;
        }
    }

    struct squeek_symbol *symbol = eek_key_get_symbol_at_index(
        key, 0);
    if (!symbol)
        return;

    set_level_from_press (keyboard, key);

    // "Borrowed" from eek-context-service; doesn't influence the state but forwards the event

    guint keycode = eek_key_get_keycode (key);

    emit_key_activated(keyboard->manager, keyboard, keycode, FALSE, timestamp);
}

static void
eek_keyboard_dispose (GObject *object)
{
    G_OBJECT_CLASS (eek_keyboard_parent_class)->dispose (object);
}

static void
eek_keyboard_finalize (GObject *object)
{
    G_OBJECT_CLASS (eek_keyboard_parent_class)->finalize (object);
}

void level_keyboard_deinit(LevelKeyboard *self) {
    g_hash_table_destroy (self->names);
    for (guint i = 0; i < self->outline_array->len; i++) {
        EekOutline *outline = &g_array_index (self->outline_array,
                                              EekOutline,
                                              i);
        g_slice_free1 (sizeof (EekPoint) * outline->num_points,
                       outline->points);
    }
    g_array_free (self->outline_array, TRUE);
    for (guint i = 0; i < 4; i++) {
        // free self->view[i];
    }
}

void level_keyboard_free(LevelKeyboard *self) {
    level_keyboard_deinit(self);
    g_free(self);
}

static void
eek_keyboard_real_child_added (EekContainer *self,
                               EekElement   *element)
{
    (void)self;
    (void)element;
}

static void
eek_keyboard_real_child_removed (EekContainer *self,
                                 EekElement   *element)
{
    (void)self;
    (void)element;
}

static void
eek_keyboard_class_init (EekKeyboardClass *klass)
{
    EekContainerClass *container_class = EEK_CONTAINER_CLASS (klass);
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);

    /* signals */
    container_class->child_added = eek_keyboard_real_child_added;
    container_class->child_removed = eek_keyboard_real_child_removed;

    gobject_class->get_property = eek_keyboard_get_property;
    gobject_class->set_property = eek_keyboard_set_property;
    gobject_class->dispose = eek_keyboard_dispose;
    gobject_class->finalize = eek_keyboard_finalize;
}

static void
eek_keyboard_init (EekKeyboard *self)
{
    self->priv = EEK_KEYBOARD_GET_PRIVATE(self);
    self->scale = 1.0;
}

void level_keyboard_init(LevelKeyboard *self) {
    self->outline_array = g_array_new (FALSE, TRUE, sizeof (EekOutline));
}

LevelKeyboard *level_keyboard_new(EekboardContextService *manager, EekKeyboard *views[4], GHashTable *name_key_hash) {
    LevelKeyboard *keyboard = g_new0(LevelKeyboard, 1);
    level_keyboard_init(keyboard);
    for (uint i = 0; i < 4; i++) {
        keyboard->views[i] = views[i];
    }
    keyboard->manager = manager;
    keyboard->names = name_key_hash;
    return keyboard;
}

/**
 * eek_keyboard_find_key_by_name:
 * @keyboard: an #EekKeyboard
 * @name: a key name
 *
 * Find an #EekKey whose name is @name.
 * Return value: (transfer none): #EekKey whose name is @name
 */
EekKey *
eek_keyboard_find_key_by_name (LevelKeyboard *keyboard,
                               const gchar *name)
{
    return g_hash_table_lookup (keyboard->names, name);
}

/**
 * eek_keyboard_get_size:
 * @keyboard: an #EekKeyboard
 * @width: width of @keyboard
 * @height: height of @keyboard
 *
 * Get the size of @keyboard.
 */
void
eek_keyboard_get_size (EekKeyboard *keyboard,
                       gdouble     *width,
                       gdouble     *height)
{
    EekBounds bounds;

    eek_element_get_bounds (EEK_ELEMENT(keyboard), &bounds);
    *width = bounds.width;
    *height = bounds.height;
}

/**
 * eek_keyboard_get_outline:
 * @keyboard: an #EekKeyboard
 * @oref: ID of the outline
 *
 * Get an outline associated with @oref in @keyboard.
 * Returns: an #EekOutline, which should not be released
 */
EekOutline *
level_keyboard_get_outline (LevelKeyboard *keyboard,
                          guint        oref)
{
    if (oref > keyboard->outline_array->len)
        return NULL;

    return &g_array_index (keyboard->outline_array, EekOutline, oref);
}

/**
 * eek_keyboard_get_keymap:
 * @keyboard: an #EekKeyboard
 *
 * Get the keymap for the keyboard.
 * Returns: a string containing the XKB keymap.
 */
gchar *
eek_keyboard_get_keymap(LevelKeyboard *keyboard)
{
    /* Start the keycodes and symbols sections with their respective headers. */
    gchar *keycodes = g_strdup(keymap_keycodes_header);
    gchar *symbols = g_strdup(keymap_symbols_header);

    /* Iterate over the keys in the name-to-key hash table. */
    GHashTableIter iter;
    gchar *key_name;
    gpointer key_ptr;
    g_hash_table_iter_init(&iter, keyboard->names);

    while (g_hash_table_iter_next(&iter, (gpointer)&key_name, &key_ptr)) {

        gchar *current, *line;
        EekKey *key = EEK_KEY(key_ptr);
        guint keycode = eek_key_get_keycode(key);

        /* Don't include invalid keycodes in the keymap. */
        if (keycode == EEK_INVALID_KEYCODE)
            continue;

        /* Append a key name-to-keycode definition to the keycodes section. */
        current = keycodes;
        line = g_strdup_printf("        <%s> = %i;\n", (char *)key_name, keycode);

        keycodes = g_strconcat(current, line, NULL);
        g_free(line);
        g_free(current);

        // FIXME: free
        const char *key_str = squeek_key_to_keymap_entry(
            (char*)key_name,
            eek_key_get_state(key)
        );
        current = symbols;
        symbols = g_strconcat(current, key_str, NULL);
        g_free(current);
    }

    /* Assemble the keymap file from the header, sections and footer. */
    gchar *keymap = g_strconcat(keymap_header,
                                keycodes, "    };\n\n",
                                symbols, "    };\n\n",
                                keymap_footer, NULL);

    g_free(keycodes);
    g_free(symbols);
    return keymap;
}

EekKeyboard *level_keyboard_current(LevelKeyboard *keyboard)
{
    return keyboard->views[keyboard->level];
}
