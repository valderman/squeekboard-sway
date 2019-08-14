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
 * SECTION:eek-section
 * @short_description: Base class of a section
 * @see_also: #EekKey
 *
 * The #EekSectionClass class represents a section, which consists
 * of one or more keys of the #EekKeyClass class.
 */

#include "config.h"

#include <string.h>

#include "eek-keyboard.h"
#include "layout.h"

#include "eek-section.h"

enum {
    PROP_0,
    PROP_ANGLE,
    PROP_LAST
};

typedef struct _EekSectionPrivate
{
    gint angle;
    EekModifierType modifiers;
    GPtrArray *buttons; // struct squeek_button*
} EekSectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekSection, eek_section, EEK_TYPE_ELEMENT)

struct squeek_button*
eek_section_create_button (EekSection *self,
                             const gchar *name,
                             guint        keycode,
                             guint oref)
{
    struct squeek_button *button = squeek_button_new(keycode, oref);
    g_return_val_if_fail (button, NULL);
    EekSectionPrivate *priv = eek_section_get_instance_private (self);
    g_ptr_array_add(priv->buttons, button);
    return button;
}

struct squeek_button *eek_section_create_button_with_state(EekSection *self,
                                  const gchar *name,
                                    struct squeek_button *source) {
    struct squeek_button *button = squeek_button_new_with_state(source);
    g_return_val_if_fail (button, NULL);
    EekSectionPrivate *priv = eek_section_get_instance_private (self);
    g_ptr_array_add(priv->buttons, button);
    return button;
}

static void
eek_section_finalize (GObject *object)
{
    G_OBJECT_CLASS (eek_section_parent_class)->finalize (object);
}

static void
eek_section_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    switch (prop_id) {
    case PROP_ANGLE:
        eek_section_set_angle (EEK_SECTION(object),
                               g_value_get_int (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_section_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_ANGLE:
        g_value_set_int (value, eek_section_get_angle (EEK_SECTION(object)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eek_section_class_init (EekSectionClass *klass)
{
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    /* signals */
    gobject_class->set_property = eek_section_set_property;
    gobject_class->get_property = eek_section_get_property;
    gobject_class->finalize     = eek_section_finalize;

    /**
     * EekSection:angle:
     *
     * The rotation angle of #EekSection.
     */
    pspec = g_param_spec_int ("angle",
                              "Angle",
                              "Rotation angle of the section",
                              -360, 360, 0,
                              G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_ANGLE,
                                     pspec);
}

static void
eek_section_init (EekSection *self)
{
    EekSectionPrivate *priv = eek_section_get_instance_private (self);
    priv->buttons = g_ptr_array_new();
}

/**
 * eek_section_set_angle:
 * @section: an #EekSection
 * @angle: rotation angle
 *
 * Set rotation angle of @section to @angle.
 */
void
eek_section_set_angle (EekSection  *section,
                       gint         angle)
{
    g_return_if_fail (EEK_IS_SECTION(section));

    EekSectionPrivate *priv = eek_section_get_instance_private (section);

    if (priv->angle != angle) {
        priv->angle = angle;
        g_object_notify (G_OBJECT(section), "angle");
    }
}

/**
 * eek_section_get_angle:
 * @section: an #EekSection
 *
 * Get rotation angle of @section.
 */
gint
eek_section_get_angle (EekSection *section)
{
    g_return_val_if_fail (EEK_IS_SECTION(section), -1);

    EekSectionPrivate *priv = eek_section_get_instance_private (section);

    return priv->angle;
}

const double keyspacing = 4.0;

struct keys_info {
    uint count;
    double total_width;
    double biggest_height;
};

/// Set button size to match the outline. Reset position
static void
buttonsizer(gpointer item, gpointer user_data)
{
    struct squeek_button *button = item;

    LevelKeyboard *keyboard = user_data;
    uint oref = squeek_button_get_oref(button);
    EekOutline *outline = level_keyboard_get_outline (keyboard, oref);
    if (outline && outline->num_points > 0) {
        double minx = outline->points[0].x;
        double maxx = minx;
        double miny = outline->points[0].y;
        double maxy = miny;
        for (uint i = 1; i < outline->num_points; i++) {
            EekPoint p = outline->points[i];
            if (p.x < minx) {
                minx = p.x;
            } else if (p.x > maxx) {
                maxx = p.x;
            }

            if (p.y < miny) {
                miny = p.y;
            } else if (p.y > maxy) {
                maxy = p.y;
            }
        }
        EekBounds key_bounds = {
            .height = maxy - miny,
            .width = maxx - minx,
            .x = 0,
            .y = 0,
        };
        squeek_button_set_bounds(button, key_bounds);
    }
}

static void
buttoncounter (gpointer item, gpointer user_data)
{
    struct squeek_button *button = item;
    struct keys_info *data = user_data;
    data->count++;
    EekBounds key_bounds = squeek_button_get_bounds(button);
    data->total_width += key_bounds.width;
    if (key_bounds.height > data->biggest_height) {
        data->biggest_height = key_bounds.height;
    }
}

static void
buttonplacer(gpointer item, gpointer user_data)
{
    struct squeek_button *button = item;
    double *current_offset = user_data;
    EekBounds key_bounds = squeek_button_get_bounds(button);
    key_bounds.x = *current_offset;
    key_bounds.y = 0;
    squeek_button_set_bounds(button, key_bounds);
    *current_offset += key_bounds.width + keyspacing;
}

void
eek_section_place_keys(EekSection *section, LevelKeyboard *keyboard)
{
    EekSectionPrivate *priv = eek_section_get_instance_private (section);

    g_ptr_array_foreach(priv->buttons, buttonsizer, keyboard);

    struct keys_info keyinfo = {0};
    g_ptr_array_foreach(priv->buttons, buttoncounter, &keyinfo);

    EekBounds section_bounds = {0};
    eek_element_get_bounds(EEK_ELEMENT(section), &section_bounds);

    double key_offset = (section_bounds.width - (keyinfo.total_width + (keyinfo.count - 1) * keyspacing)) / 2;
    g_ptr_array_foreach(priv->buttons, buttonplacer, &key_offset);

    section_bounds.height = keyinfo.biggest_height;
    eek_element_set_bounds(EEK_ELEMENT(section), &section_bounds);
}

void eek_section_foreach (EekSection *section,
                     GFunc      func,
                          gpointer   user_data) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    g_ptr_array_foreach(priv->buttons, func, user_data);
}

gboolean eek_section_find(EekSection *section,
                          const struct squeek_button *button) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    return g_ptr_array_find(priv->buttons, button, NULL);
}

static gboolean button_has_key(gconstpointer buttonptr, gconstpointer keyptr) {
    const struct squeek_button *button = buttonptr;
    const struct squeek_key *key = keyptr;
    return squeek_button_has_key((struct squeek_button*)button, key) != 0;
}

struct squeek_button *eek_section_find_key(EekSection *section,
                                           const struct squeek_key *key) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    guint index;
    gboolean ret = g_ptr_array_find_with_equal_func(priv->buttons, key, button_has_key, &index);
    if (ret) {
        return g_ptr_array_index(priv->buttons, index);
    }
    return NULL;// TODO: store keys in locked_keys, pressed_keys
}
