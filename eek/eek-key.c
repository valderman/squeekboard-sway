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
 * SECTION:eek-key
 * @short_description: Base class of a key
 *
 * The #EekKeyClass class represents a key.
 */

#include "config.h"

#include <string.h>

#include "eek-section.h"
#include "eek-keyboard.h"
#include "src/keyboard.h"
#include "src/symbol.h"

#include "eek-key.h"

enum {
    PROP_0,
    PROP_OREF,
    PROP_LAST
};

typedef struct _EekKeyPrivate
{
    gulong oref; // UI outline reference
    struct squeek_key *state;
} EekKeyPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekKey, eek_key, EEK_TYPE_ELEMENT)

void
eek_key_set_locked (EekKey *self, gboolean value)
{
    EekKeyPrivate *priv = eek_key_get_instance_private (self);
    squeek_key_set_pressed(priv->state, value);
}

static void
eek_key_finalize (GObject *object)
{
    EekKey        *self = EEK_KEY (object);
    EekKeyPrivate *priv = eek_key_get_instance_private (self);

    squeek_key_free (priv->state);

    G_OBJECT_CLASS (eek_key_parent_class)->finalize (object);
}

static void
eek_key_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
    switch (prop_id) {
    case PROP_OREF:
        eek_key_set_oref (EEK_KEY(object), g_value_get_uint (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_key_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_OREF:
        g_value_set_uint (value, eek_key_get_oref (EEK_KEY(object)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_key_class_init (EekKeyClass *klass)
{
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    gobject_class->set_property = eek_key_set_property;
    gobject_class->get_property = eek_key_get_property;
    gobject_class->finalize     = eek_key_finalize;

    /**
     * EekKey:oref:
     *
     * The outline id of #EekKey.
     */
    pspec = g_param_spec_ulong ("oref",
                                "Oref",
                                "Outline id of the key",
                                0, G_MAXULONG, 0,
                                G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_OREF, pspec);
}

static void
eek_key_init (EekKey *self)
{
    EekKeyPrivate *priv = eek_key_get_instance_private (self);
    priv->state = squeek_key_new (0);
}

void eek_key_share_state(EekKey *self, struct squeek_key *state) {
    EekKeyPrivate *priv = eek_key_get_instance_private (self);
    priv->state = state;
}
/**
 * eek_key_set_keycode:
 * @key: an #EekKey
 * @keycode: keycode
 *
 * Set the keycode of @key to @keycode.  Since typically the keycode
 * value is used to find a key in a keyboard by calling
 * eek_keyboard_find_key_by_keycode, it is not necessarily the same as
 * the X keycode but it should be unique in the keyboard @key belongs
 * to.
 */
void
eek_key_set_keycode (EekKey *key,
                     guint   keycode)
{
    g_return_if_fail (EEK_IS_KEY (key));

    EekKeyPrivate *priv = eek_key_get_instance_private (key);

    squeek_key_set_keycode(priv->state, keycode);
}

/**
 * eek_key_get_keycode:
 * @key: an #EekKey
 *
 * Get keycode of @key.
 * Returns: keycode or %EEK_INVALID_KEYCODE on failure
 */
guint
eek_key_get_keycode (EekKey *key)
{
    g_return_val_if_fail (EEK_IS_KEY (key), EEK_INVALID_KEYCODE);

    EekKeyPrivate *priv = eek_key_get_instance_private (key);

    return squeek_key_get_keycode(priv->state);
}

/**
 * eek_key_get_symbol_at_index:
 * @key: an #EekKey
 * @group: group index of the symbol matrix
 * @level: level index of the symbol matrix
 * @fallback_group: fallback group index
 * @fallback_level: fallback level index
 *
 * Get the symbol at (@group, @level) in the symbol matrix of @key.
 * Return value: (transfer none): an #EekSymbol at (@group, @level), or %NULL
 */
struct squeek_symbol*
eek_key_get_symbol_at_index (EekKey *key,
                             gint    group,
                             guint    level)
{
    EekKeyPrivate *priv = eek_key_get_instance_private (key);
    return squeek_key_get_symbol(priv->state, level);
}

/**
 * eek_key_set_oref:
 * @key: an #EekKey
 * @oref: outline id of @key
 *
 * Set the outline id of @key to @oref.
 */
void
eek_key_set_oref (EekKey *key,
                  guint   oref)
{
    g_return_if_fail (EEK_IS_KEY(key));

    EekKeyPrivate *priv = eek_key_get_instance_private (key);

    if (priv->oref != oref) {
        priv->oref = oref;
        g_object_notify (G_OBJECT(key), "oref");
    }
}

/**
 * eek_key_get_oref:
 * @key: an #EekKey
 *
 * Get the outline id of @key.
 * Returns: unsigned integer
 */
guint
eek_key_get_oref (EekKey *key)
{
    g_return_val_if_fail (EEK_IS_KEY (key), 0);

    EekKeyPrivate *priv = eek_key_get_instance_private (key);

    return priv->oref;
}

/**
 * eek_key_is_pressed:
 * @key: an #EekKey
 *
 * Return %TRUE if key is marked as pressed.
 */
gboolean
eek_key_is_pressed (EekKey *key)
{
    g_return_val_if_fail (EEK_IS_KEY(key), FALSE);

    EekKeyPrivate *priv = (EekKeyPrivate*)eek_key_get_instance_private (key);

    return (bool)squeek_key_is_pressed(priv->state);
}

/**
 * eek_key_is_locked:
 * @key: an #EekKey
 *
 * Return %TRUE if key is marked as locked.
 */
gboolean
eek_key_is_locked (EekKey *key)
{
    g_return_val_if_fail (EEK_IS_KEY(key), FALSE);

    EekKeyPrivate *priv = eek_key_get_instance_private (key);

    return (bool)squeek_key_is_locked(priv->state);
}

void eek_key_set_pressed(EekKey *key, gboolean value)
{
    g_return_if_fail (EEK_IS_KEY(key));

    EekKeyPrivate *priv = eek_key_get_instance_private (key);

    squeek_key_set_pressed(priv->state, value);
}

struct squeek_key *eek_key_get_state(EekKey *key) {
    EekKeyPrivate *priv = eek_key_get_instance_private (key);
    return priv->state;
}
