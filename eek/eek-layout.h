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

#ifndef EEK_LAYOUT_H
#define EEK_LAYOUT_H 1

#include <glib-object.h>
#include "eek-types.h"

G_BEGIN_DECLS

#define EEK_TYPE_LAYOUT (eek_layout_get_type())
G_DECLARE_DERIVABLE_TYPE (EekLayout, eek_layout, EEK, LAYOUT, GObject)

/**
 * EekLayoutClass:
 * @create_keyboard: virtual function for creating a keyboard
 */
struct _EekLayoutClass
{
    /*< private >*/
    GObjectClass parent_class;

    /*< public >*/
    EekKeyboard* (* create_keyboard) (EekboardContextService *manager,
                                      EekLayout *self,
                                      gdouble    initial_width,
                                      gdouble    initial_height);

    /*< private >*/
    /* padding */
    gpointer pdummy[24];
};

GType        eek_layout_get_type  (void) G_GNUC_CONST;

void         eek_layout_place_sections(EekKeyboard *keyboard);

G_END_DECLS
#endif  /* EEK_LAYOUT_H */
