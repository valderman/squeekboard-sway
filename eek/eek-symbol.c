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

enum {
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_MODIFIER_MASK,
    PROP_ICON_NAME,
    PROP_TOOLTIP,
    PROP_LAST
};

typedef struct _EekSymbolPrivate
{
    gchar *name;
    gchar *label;
    EekModifierType modifier_mask;
    gchar *icon_name;
    gchar *tooltip;
} EekSymbolPrivate;

G_DEFINE_TYPE_EXTENDED (EekSymbol,
			eek_symbol,
			G_TYPE_OBJECT,
			0, /* GTypeFlags */
            G_ADD_PRIVATE (EekSymbol))

static void
eek_symbol_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    switch (prop_id) {
    case PROP_NAME:
        eek_symbol_set_name (EEK_SYMBOL(object), g_value_get_string (value));
        break;
    case PROP_LABEL:
        eek_symbol_set_label (EEK_SYMBOL(object), g_value_get_string (value));
        break;
    case PROP_MODIFIER_MASK:
        eek_symbol_set_modifier_mask (EEK_SYMBOL(object),
                                      g_value_get_flags (value));
        break;
    case PROP_ICON_NAME:
        eek_symbol_set_icon_name (EEK_SYMBOL(object),
                                  g_value_get_string (value));
        break;
    case PROP_TOOLTIP:
        eek_symbol_set_tooltip (EEK_SYMBOL(object),
                                g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_symbol_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, eek_symbol_get_name (EEK_SYMBOL(object)));
        break;
    case PROP_LABEL:
        g_value_set_string (value, eek_symbol_get_label (EEK_SYMBOL(object)));
        break;
    case PROP_MODIFIER_MASK:
        g_value_set_flags (value,
                           eek_symbol_get_modifier_mask (EEK_SYMBOL(object)));
        break;
    case PROP_ICON_NAME:
        g_value_set_string (value,
                            eek_symbol_get_icon_name (EEK_SYMBOL(object)));
        break;
    case PROP_TOOLTIP:
        g_value_set_string (value,
                            eek_symbol_get_tooltip (EEK_SYMBOL(object)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_symbol_finalize (GObject *object)
{
    EekSymbol        *self = EEK_SYMBOL (object);
    EekSymbolPrivate *priv = eek_symbol_get_instance_private (self);

    g_free (priv->name);
    g_free (priv->label);
    g_free (priv->icon_name);
    g_free (priv->tooltip);
    G_OBJECT_CLASS (eek_symbol_parent_class)->finalize (object);
}

static void
eek_symbol_class_init (EekSymbolClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->set_property = eek_symbol_set_property;
    gobject_class->get_property = eek_symbol_get_property;
    gobject_class->finalize = eek_symbol_finalize;

    pspec = g_param_spec_string ("name",
                                 "Name",
                                 "Canonical name of the symbol",
                                 NULL,
                                 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_NAME, pspec);

    pspec = g_param_spec_string ("label",
                                 "Label",
                                 "Text used to display the symbol",
                                 NULL,
                                 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_LABEL, pspec);

    pspec = g_param_spec_flags ("modifier-mask",
                                "Modifier mask",
                                "Modifier mask of the symbol",
                                EEK_TYPE_MODIFIER_TYPE,
                                0,
                                G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_MODIFIER_MASK, pspec);

    pspec = g_param_spec_string ("icon-name",
                                 "Icon name",
                                 "Icon name used to render the symbol",
                                 NULL,
                                 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_ICON_NAME, pspec);

    pspec = g_param_spec_string ("tooltip",
                                 "Tooltip",
                                 "Tooltip text",
                                 NULL,
                                 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_TOOLTIP, pspec);
}

static void
eek_symbol_init (EekSymbol *self)
{}

/**
 * eek_symbol_new:
 * @name: name of the symbol
 *
 * Create a new #EekSymbol with @name.
 */
EekSymbol *
eek_symbol_new (const gchar *name)
{
    return g_object_new (EEK_TYPE_SYMBOL, "name", name, NULL);
}

/**
 * eek_symbol_set_name:
 * @symbol: an #EekSymbol
 * @name: name of the symbol
 *
 * Set the name of @symbol to @name.
 */
void
eek_symbol_set_name (EekSymbol   *symbol,
                     const gchar *name)
{
    g_return_if_fail (EEK_IS_SYMBOL(symbol));

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    g_free (priv->name);
    priv->name = g_strdup (name);
}

/**
 * eek_symbol_get_name:
 * @symbol: an #EekSymbol
 *
 * Get the name of @symbol.
 */
const gchar *
eek_symbol_get_name (EekSymbol *symbol)
{
    g_return_val_if_fail (EEK_IS_SYMBOL(symbol), NULL);

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    if (priv->name == NULL || *priv->name == '\0')
        return NULL;
    return priv->name;
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
    g_return_if_fail (EEK_IS_SYMBOL(symbol));

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    g_free (priv->label);
    priv->label = g_strdup (label);
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
    g_return_val_if_fail (EEK_IS_SYMBOL(symbol), NULL);

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    if (priv->label == NULL || *priv->label == '\0')
        return NULL;
    return priv->label;
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
    g_return_if_fail (EEK_IS_SYMBOL(symbol));

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    priv->modifier_mask = mask;
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
    g_return_val_if_fail (EEK_IS_SYMBOL(symbol), 0);

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    return priv->modifier_mask;
}

/**
 * eek_symbol_is_modifier:
 * @symbol: an #EekSymbol
 *
 * Check if @symbol is a modifier.
 * Returns: %TRUE if @symbol is a modifier.
 */
gboolean
eek_symbol_is_modifier (EekSymbol *symbol)
{
    return eek_symbol_get_modifier_mask (symbol) != 0;
}

/**
 * eek_symbol_set_icon_name:
 * @symbol: an #EekSymbol
 * @icon_name: icon name of @symbol
 *
 * Set the icon name of @symbol to @icon_name.
 */
void
eek_symbol_set_icon_name (EekSymbol   *symbol,
                          const gchar *icon_name)
{
    g_return_if_fail (EEK_IS_SYMBOL(symbol));

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    g_free (priv->icon_name);
    priv->icon_name = g_strdup (icon_name);
}

/**
 * eek_symbol_get_icon_name:
 * @symbol: an #EekSymbol
 *
 * Get the icon name of @symbol.
 */
const gchar *
eek_symbol_get_icon_name (EekSymbol *symbol)
{
    g_return_val_if_fail (EEK_IS_SYMBOL(symbol), NULL);

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    if (priv->icon_name == NULL || *priv->icon_name == '\0')
        return NULL;
    return priv->icon_name;
}

/**
 * eek_symbol_set_tooltip:
 * @symbol: an #EekSymbol
 * @tooltip: icon name of @symbol
 *
 * Set the tooltip text of @symbol to @tooltip.
 */
void
eek_symbol_set_tooltip (EekSymbol   *symbol,
                        const gchar *tooltip)
{
    g_return_if_fail (EEK_IS_SYMBOL(symbol));

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    g_free (priv->tooltip);
    priv->tooltip = g_strdup (tooltip);
}

/**
 * eek_symbol_get_tooltip:
 * @symbol: an #EekSymbol
 *
 * Get the tooltip text of @symbol.
 */
const gchar *
eek_symbol_get_tooltip (EekSymbol *symbol)
{
    g_return_val_if_fail (EEK_IS_SYMBOL(symbol), NULL);

    EekSymbolPrivate *priv = eek_symbol_get_instance_private (symbol);

    if (priv->tooltip == NULL || *priv->tooltip == '\0')
        return NULL;
    return priv->tooltip;
}

