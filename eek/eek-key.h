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

#ifndef EEK_KEY_H
#define EEK_KEY_H 1

#include "eek-element.h"

G_BEGIN_DECLS

#define EEK_TYPE_KEY (eek_key_get_type())
G_DECLARE_DERIVABLE_TYPE(EekKey, eek_key, EEK, KEY, EekElement)

/**
 * EekKeyClass:
 * @pressed: class handler for #EekKey::pressed signal
 * @released: class handler for #EekKey::released signal
 * @locked: class handler for #EekKey::locked signal
 * @unlocked: class handler for #EekKey::unlocked signal
 * @cancelled: class handler for #EekKey::cancelled signal
 * @is_pressed: virtual function for getting whether the key is pressed
 * @is_locked: virtual function for getting whether the key is locked
 */
struct _EekKeyClass
{
    /*< private >*/
    EekElementClass parent_class;
};

GType            eek_key_get_type            (void) G_GNUC_CONST;

void             eek_key_set_keycode         (EekKey          *key,
                                              guint            keycode);
guint            eek_key_get_keycode         (EekKey          *key);
struct squeek_key *eek_key_get_state(EekKey *key);
struct squeek_symbol *eek_key_get_symbol_at_index (EekKey          *key,
                                              gint             group,
                                              guint level);

void             eek_key_set_oref            (EekKey          *key,
                                              guint            oref);
guint            eek_key_get_oref            (EekKey          *key);

gboolean         eek_key_is_pressed          (EekKey          *key);
gboolean         eek_key_is_locked           (EekKey          *key);
void             eek_key_set_pressed         (EekKey          *key,
                                              gboolean         value);
void
eek_key_set_locked (EekKey *self, gboolean value);
void eek_key_share_state(EekKey *self, struct squeek_key *state);
G_END_DECLS
#endif  /* EEK_KEY_H */
