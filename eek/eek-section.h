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

#ifndef EEK_SECTION_H
#define EEK_SECTION_H 1

#include <glib-object.h>
#include "eek-container.h"
#include "eek-types.h"

G_BEGIN_DECLS

#define EEK_TYPE_SECTION (eek_section_get_type())
G_DECLARE_DERIVABLE_TYPE(EekSection, eek_section, EEK, SECTION, EekContainer)

/**
 * EekSectionClass:
 * @get_n_rows: virtual function for getting the number of rows in the section
 * @add_row: virtual function for adding a new row to the section
 * @get_row: virtual function for accessing a row in the section
 * @create_key: virtual function for creating key in the section
 * @key_pressed: class handler for #EekSection::key-pressed signal
 * @key_released: class handler for #EekSection::key-released signal
 * @key_locked: class handler for #EekSection::key-locked signal
 * @key_unlocked: class handler for #EekSection::key-unlocked signal
 * @key_cancelled: class handler for #EekSection::key-cancelled signal
 */
struct _EekSectionClass
{
    /*< private >*/
    EekContainerClass parent_class;

    /*< public >*/
    gint    (* get_n_rows)          (EekSection     *self);
    void    (* add_row)             (EekSection     *self,
                                     gint            num_columns,
                                     EekOrientation  orientation);
    void    (* get_row)             (EekSection     *self,
                                     gint            index,
                                     gint           *num_columns,
                                     EekOrientation *orientation);

    EekKey *(* create_key)          (EekSection     *self,
                                     const gchar    *name,
                                     gint            keycode);

    /* signals */
    void    (* key_locked)          (EekSection     *self,
                                     EekKey         *key);
    void    (* key_unlocked)        (EekSection     *self,
                                     EekKey         *key);
    void    (* key_cancelled)       (EekSection     *self,
                                     EekKey         *key);

    /*< private >*/
    /* padding */
    gpointer pdummy[19];
};

GType   eek_section_get_type             (void) G_GNUC_CONST;

void    eek_section_set_angle            (EekSection     *section,
                                          gint            angle);
gint    eek_section_get_angle            (EekSection     *section);

gint    eek_section_get_n_rows           (EekSection     *section);
void    eek_section_add_row              (EekSection     *section,
                                          gint            num_columns,
                                          EekOrientation  orientation);
void    eek_section_get_row              (EekSection     *section,
                                          gint            index,
                                          gint           *num_columns,
                                          EekOrientation *orientation);

EekKey *eek_section_create_key           (EekSection     *section,
                                          const gchar    *name,
                                          guint keycode);

void eek_section_place_keys              (EekSection     *section, EekKeyboard *keyboard);

G_END_DECLS
#endif  /* EEK_SECTION_H */
