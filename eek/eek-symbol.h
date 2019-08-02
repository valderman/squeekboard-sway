/* 
 * Copyright (C) 2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2011 Red Hat, Inc.
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

#ifndef EEK_SYMBOL_H
#define EEK_SYMBOL_H 1

#include "eek-types.h"

G_BEGIN_DECLS

#define EEK_TYPE_SYMBOL (eek_symbol_get_type())
G_DECLARE_DERIVABLE_TYPE(EekSymbol, eek_symbol, EEK, SYMBOL, GObject)

/**
 * EekSymbolClass:
 */
struct _EekSymbolClass {
    /*< private >*/
    GObjectClass parent_class;
};

GType             eek_symbol_get_type           (void) G_GNUC_CONST;

EekSymbol        *eek_symbol_new                (const gchar      *name);
void              eek_symbol_set_name           (EekSymbol        *symbol,
                                                 const gchar      *name);
const gchar      *eek_symbol_get_name           (EekSymbol        *symbol);
void              eek_symbol_set_label          (EekSymbol        *symbol,
                                                 const gchar      *label);
const gchar      *eek_symbol_get_label          (EekSymbol        *symbol);
EekModifierType   eek_symbol_get_modifier_mask  (EekSymbol        *symbol);
void              eek_symbol_set_modifier_mask  (EekSymbol        *symbol,
                                                 EekModifierType   mask);
gboolean          eek_symbol_is_modifier        (EekSymbol        *symbol);
void              eek_symbol_set_icon_name      (EekSymbol        *symbol,
                                                 const gchar      *icon_name);
const gchar      *eek_symbol_get_icon_name      (EekSymbol        *symbol);
void              eek_symbol_set_tooltip        (EekSymbol        *symbol,
                                                 const gchar      *tooltip);
const gchar *     eek_symbol_get_tooltip        (EekSymbol        *symbol);

G_END_DECLS

#endif  /* EEK_SYMBOL_H */
