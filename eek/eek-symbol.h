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
#include "src/symbol.h"


G_BEGIN_DECLS

/**
 * EekSymbolCategory:
 * @EEK_SYMBOL_CATEGORY_LETTER: the symbol represents an alphabet letter
 * @EEK_SYMBOL_CATEGORY_FUNCTION: the symbol represents a function
 * @EEK_SYMBOL_CATEGORY_KEYNAME: the symbol does not have meaning but
 * have a name
 * @EEK_SYMBOL_CATEGORY_USER0: reserved for future use
 * @EEK_SYMBOL_CATEGORY_USER1: reserved for future use
 * @EEK_SYMBOL_CATEGORY_USER2: reserved for future use
 * @EEK_SYMBOL_CATEGORY_USER3: reserved for future use
 * @EEK_SYMBOL_CATEGORY_USER4: reserved for future use
 * @EEK_SYMBOL_CATEGORY_UNKNOWN: used for error reporting
 * @EEK_SYMBOL_CATEGORY_LAST: the last symbol category
 *
 * Category of the key symbols.
 */
typedef enum {
    EEK_SYMBOL_CATEGORY_LETTER,
    EEK_SYMBOL_CATEGORY_FUNCTION,
    EEK_SYMBOL_CATEGORY_KEYNAME,
    EEK_SYMBOL_CATEGORY_USER0,
    EEK_SYMBOL_CATEGORY_USER1,
    EEK_SYMBOL_CATEGORY_USER2,
    EEK_SYMBOL_CATEGORY_USER3,
    EEK_SYMBOL_CATEGORY_USER4,
    EEK_SYMBOL_CATEGORY_UNKNOWN,
    EEK_SYMBOL_CATEGORY_LAST = EEK_SYMBOL_CATEGORY_UNKNOWN
} EekSymbolCategory;


typedef struct _EekSymbol
{
    /// Canonical name of the symbol
    gchar *name;
    /// Text used to display the symbol
    gchar *label;
    EekSymbolCategory category;
    EekModifierType modifier_mask;
    /// Icon name used to render the symbol
    gchar *icon_name;
    /// Tooltip text
    gchar *tooltip;

    // May not be present
    guint xkeysym;
    gchar *text;
} EekSymbol;

EekSymbol        *eek_symbol_new                (const gchar      *name);
void              eek_symbol_free               (EekSymbol        *symbol);
void              eek_symbol_set_name           (EekSymbol        *symbol,
                                                 const gchar      *name);
const gchar      *eek_symbol_get_name           (EekSymbol        *symbol);
void              eek_symbol_set_label          (EekSymbol        *symbol,
                                                 const gchar      *label);
const gchar      *eek_symbol_get_label          (EekSymbol        *symbol);
void              eek_symbol_set_category       (EekSymbol        *symbol,
                                                 EekSymbolCategory category);
EekSymbolCategory eek_symbol_get_category       (EekSymbol        *symbol);
EekModifierType   eek_symbol_get_modifier_mask  (EekSymbol        *symbol);
void              eek_symbol_set_modifier_mask  (EekSymbol        *symbol,
                                                 EekModifierType   mask);
void              eek_symbol_set_icon_name      (EekSymbol        *symbol,
                                                 const gchar      *icon_name);
const gchar      *eek_symbol_get_icon_name      (EekSymbol        *symbol);
void              eek_symbol_set_tooltip        (EekSymbol        *symbol,
                                                 const gchar      *tooltip);
const gchar *     eek_symbol_get_tooltip        (EekSymbol        *symbol);

const gchar      *eek_symbol_category_get_name  (EekSymbolCategory category);
EekSymbolCategory eek_symbol_category_from_name (const gchar      *name);

G_END_DECLS

#endif  /* EEK_SYMBOL_H */
