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
 * SECTION:eek-text
 * @short_description: an #EekText represents a text symbol
 */

#include "config.h"

#include "eek-text.h"
#include "eek-serializable.h"

enum {
    PROP_0,
    PROP_TEXT,
    PROP_LAST
};

typedef struct _EekTextPrivate
{
    gchar *text;
} EekTextPrivate;

static void eek_serializable_iface_init (EekSerializableIface *iface);

G_DEFINE_TYPE_EXTENDED (EekText,
			eek_text,
			EEK_TYPE_SYMBOL,
			0, /* GTypeFlags */
			G_ADD_PRIVATE (EekText)
                        G_IMPLEMENT_INTERFACE (EEK_TYPE_SERIALIZABLE,
                                               eek_serializable_iface_init))

static EekSerializableIface *eek_text_parent_serializable_iface;

static void
eek_text_real_serialize (EekSerializable *self,
                         GVariantBuilder *builder)
{
    EekTextPrivate *priv = eek_text_get_instance_private (EEK_TEXT (self));

    eek_text_parent_serializable_iface->serialize (self, builder);

    g_variant_builder_add (builder, "s", priv->text);
}

static gsize
eek_text_real_deserialize (EekSerializable *self,
                           GVariant        *variant,
                           gsize            index)
{
    EekTextPrivate *priv = eek_text_get_instance_private (EEK_TEXT (self));

    index = eek_text_parent_serializable_iface->deserialize (self,
                                                             variant,
                                                             index);
    g_variant_get_child (variant, index++, "s", &priv->text);

    return index;
}

static void
eek_serializable_iface_init (EekSerializableIface *iface)
{
    eek_text_parent_serializable_iface =
        g_type_interface_peek_parent (iface);

    iface->serialize = eek_text_real_serialize;
    iface->deserialize = eek_text_real_deserialize;
}

static void
eek_text_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    EekText        *self = EEK_TEXT (object);
    EekTextPrivate *priv = eek_text_get_instance_private (self);

    switch (prop_id) {
    case PROP_TEXT:
        g_free (priv->text);
        priv->text = g_strdup (g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_text_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
    EekText        *self = EEK_TEXT (object);
    EekTextPrivate *priv = eek_text_get_instance_private (self);

    switch (prop_id) {
    case PROP_TEXT:
        g_value_set_string (value, priv->text);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_text_finalize (GObject *object)
{
    EekText        *self = EEK_TEXT (object);
    EekTextPrivate *priv = eek_text_get_instance_private (self);

    g_free (priv->text);
    G_OBJECT_CLASS (eek_text_parent_class)->finalize (object);
}

static void
eek_text_class_init (EekTextClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->set_property = eek_text_set_property;
    gobject_class->get_property = eek_text_get_property;
    gobject_class->finalize = eek_text_finalize;

    pspec = g_param_spec_string ("text",
                                 "Text",
                                 "Text",
                                 NULL,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_TEXT, pspec);
}

static void
eek_text_init (EekText *self)
{
    /* void */
}

EekText *
eek_text_new (const gchar *text)
{
    return g_object_new (EEK_TYPE_TEXT,
                         "label", text,
                         "category", EEK_SYMBOL_CATEGORY_FUNCTION,
                         "text", text,
                         NULL);
}

/**
 * eek_text_get_text:
 * @text: an #EekText
 *
 * Get a text value associated with @text
 */
const gchar *
eek_text_get_text (EekText *text)
{
    EekTextPrivate *priv = eek_text_get_instance_private (text);

    return priv->text;
}
