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
 * SECTION:eek-element
 * @short_description: Base class of a keyboard element
 *
 * The #EekElementClass class represents a keyboard element, which
 * shall be used to implement #EekKeyboard, #EekSection, or #EekKey.
 */

#include "config.h"

#include <string.h>

#include "eek-element.h"

enum {
    PROP_0,
    PROP_NAME,
    PROP_BOUNDS,
    PROP_LAST
};

typedef struct _EekElementPrivate
{
    gchar *name;
    EekBounds bounds;
} EekElementPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EekElement, eek_element, G_TYPE_OBJECT)

static void
eek_element_finalize (GObject *object)
{
    EekElement        *self = EEK_ELEMENT (object);
    EekElementPrivate *priv = eek_element_get_instance_private (self);

    g_free (priv->name);
    G_OBJECT_CLASS (eek_element_parent_class)->finalize (object);
}

static void
eek_element_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    EekElement *element = EEK_ELEMENT(object);

    switch (prop_id) {
    case PROP_NAME:
        eek_element_set_name (element,
                              g_value_dup_string (value));
        break;
    case PROP_BOUNDS:
        eek_element_set_bounds (element, g_value_get_boxed (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_element_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    EekElement *element = EEK_ELEMENT(object);
    EekBounds bounds;

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, eek_element_get_name (element));
        break;
    case PROP_BOUNDS:
        eek_element_get_bounds (element, &bounds);
        g_value_set_boxed (value, &bounds);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_element_class_init (EekElementClass *klass)
{
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    /* signals */
    gobject_class->set_property = eek_element_set_property;
    gobject_class->get_property = eek_element_get_property;
    gobject_class->finalize     = eek_element_finalize;

    /**
     * EekElement:name:
     *
     * The name of #EekElement.
     */
    pspec = g_param_spec_string ("name",
                                 "Name",
                                 "Name",
                                 NULL,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     pspec);

    /**
     * EekElement:bounds:
     *
     * The bounding box of #EekElement.
     */
    pspec = g_param_spec_boxed ("bounds",
                                "Bounds",
                                "Bounding box of the element",
                                EEK_TYPE_BOUNDS,
                                G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_BOUNDS,
                                     pspec);
}

static void
eek_element_init (EekElement *self)
{
    (void)self;
}

/**
 * eek_element_set_name:
 * @element: an #EekElement
 * @name: name of @element
 *
 * Set the name of @element to @name.
 */
void
eek_element_set_name (EekElement  *element,
                      const gchar *name)
{
    g_return_if_fail (EEK_IS_ELEMENT(element));

    EekElementPrivate *priv = eek_element_get_instance_private (element);

    g_free (priv->name);
    priv->name = g_strdup (name);
}

/**
 * eek_element_get_name:
 * @element: an #EekElement
 *
 * Get the name of @element.
 * Returns: the name of @element or NULL when the name is not set
 */
const gchar *
eek_element_get_name (EekElement  *element)
{
    g_return_val_if_fail (EEK_IS_ELEMENT(element), NULL);

    EekElementPrivate *priv = eek_element_get_instance_private (element);

    return priv->name;
}

/**
 * eek_element_set_bounds:
 * @element: an #EekElement
 * @bounds: bounding box of @element
 *
 * Set the bounding box of @element to @bounds.  Note that if @element
 * has parent, X and Y positions of @bounds are relative to the parent
 * position.
 */
void
eek_element_set_bounds (EekElement  *element,
                        EekBounds   *bounds)
{
    g_return_if_fail (EEK_IS_ELEMENT(element));

    EekElementPrivate *priv = eek_element_get_instance_private (element);

    memcpy (&priv->bounds, bounds, sizeof(EekBounds));
}

/**
 * eek_element_get_bounds:
 * @element: an #EekElement
 * @bounds: (out): pointer where bounding box of @element will be stored
 *
 * Get the bounding box of @element.  Note that if @element has
 * parent, position of @bounds are relative to the parent.  To obtain
 * the absolute position, use eek_element_get_absolute_position().
 */
void
eek_element_get_bounds (EekElement  *element,
                        EekBounds   *bounds)
{
    g_return_if_fail (EEK_IS_ELEMENT(element));
    g_return_if_fail (bounds != NULL);

    EekElementPrivate *priv = eek_element_get_instance_private (element);

    memcpy (bounds, &priv->bounds, sizeof(EekBounds));
}

/**
 * eek_element_set_position:
 * @element: an #EekElement
 * @x: X coordinate of top left corner
 * @y: Y coordinate of top left corner
 *
 * Set the relative position of @element.
 */
void
eek_element_set_position (EekElement *element,
                          gdouble     x,
                          gdouble     y)
{
    EekBounds bounds;

    eek_element_get_bounds (element, &bounds);
    bounds.x = x;
    bounds.y = y;
    eek_element_set_bounds (element, &bounds);
}

/**
 * eek_element_set_size:
 * @element: an #EekElement
 * @width: width of @element
 * @height: height of @element
 *
 * Set the size of @element.
 */
void
eek_element_set_size (EekElement  *element,
                      gdouble      width,
                      gdouble      height)
{
    EekBounds bounds;

    eek_element_get_bounds (element, &bounds);
    bounds.width = width;
    bounds.height = height;
    eek_element_set_bounds (element, &bounds);
}
