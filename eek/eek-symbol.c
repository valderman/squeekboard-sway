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

/**
 * SECTION:eek-symbol
 * @short_description: Base class of a symbol
 *
 * The #EekSymbolClass class represents a symbol assigned to a key.
 */

#include "config.h"

#include "eek-symbol.h"
#include "eek-enumtypes.h"

void
eek_symbol_destroy (EekSymbol *priv)
{
    g_free (priv->name);
    g_free (priv->label);
    g_free (priv->icon_name);
    g_free (priv->tooltip);
    g_free(priv->text);
    g_free(priv);
}

EekSymbol *
eek_symbol_new (const gchar *name)
{
    EekSymbol *self = g_new0(EekSymbol, 1);
    eek_symbol_set_name(self, name);
    self->category = EEK_SYMBOL_CATEGORY_UNKNOWN;
    return self;
}

void
eek_symbol_set_name (EekSymbol   *symbol,
                     const gchar *name)
{
    g_free (symbol->name);
    symbol->name = g_strdup (name);
}

const gchar *
eek_symbol_get_name (EekSymbol *symbol)
{
    if (symbol->name == NULL || *symbol->name == '\0')
        return NULL;
    return symbol->name;
}

/**
 * eek_symbol_set_label:
 * @symbol: an #EekSymbol
 * @label: label text of @symbol
 *
 * Set the label text of @symbol to @label.
 */
void
eek_symbol_set_label (EekSymbol   *symbol,
                      const gchar *label)
{
    g_free (symbol->label);
    symbol->label = g_strdup (label);
}

/**
 * eek_symbol_get_label:
 * @symbol: an #EekSymbol
 *
 * Get the label text of @symbol.
 */
const gchar *
eek_symbol_get_label (EekSymbol *symbol)
{
    if (symbol->label == NULL || *symbol->label == '\0')
        return NULL;
    return symbol->label;
}

/**
 * eek_symbol_set_modifier_mask:
 * @symbol: an #EekSymbol
 * @mask: an #EekModifierType
 *
 * Set modifier mask that @symbol can trigger.
 */
void
eek_symbol_set_modifier_mask (EekSymbol      *symbol,
                              EekModifierType mask)
{
    symbol->modifier_mask = mask;
}

/**
 * eek_symbol_get_modifier_mask:
 * @symbol: an #EekSymbol
 *
 * Get modifier mask that @symbol can trigger.
 */
EekModifierType
eek_symbol_get_modifier_mask (EekSymbol *symbol)
{
    return symbol->modifier_mask;
}

gboolean
eek_symbol_is_modifier (EekSymbol *symbol)
{
    return eek_symbol_get_modifier_mask (symbol) != 0;
}

void
eek_symbol_set_icon_name (EekSymbol   *symbol,
                          const gchar *icon_name)
{
    g_free (symbol->icon_name);
    symbol->icon_name = g_strdup (icon_name);
}

const gchar *
eek_symbol_get_icon_name (EekSymbol *symbol)
{
    if (symbol->icon_name == NULL || *symbol->icon_name == '\0')
        return NULL;
    return symbol->icon_name;
}

void
eek_symbol_set_tooltip (EekSymbol   *symbol,
                        const gchar *tooltip)
{
    g_free (symbol->tooltip);
    symbol->tooltip = g_strdup (tooltip);
}

const gchar *
eek_symbol_get_tooltip (EekSymbol *symbol)
{
    if (symbol->tooltip == NULL || *symbol->tooltip == '\0')
        return NULL;
    return symbol->tooltip;
}
