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
    struct squeek_row *row;
} EekSectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekSection, eek_section, EEK_TYPE_ELEMENT)

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
    priv->row = squeek_row_new(0);
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

    squeek_row_set_angle(priv->row, angle);
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

    return squeek_row_get_angle(priv->row);
}

struct squeek_row *
eek_section_get_row (EekSection *section)
{
    g_return_val_if_fail (EEK_IS_SECTION(section), NULL);

    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    return priv->row;
}

EekBounds eek_get_outline_size(LevelKeyboard *keyboard, uint32_t oref) {
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
        return key_bounds;
    }
    EekBounds bounds = {0, 0, 0, 0};
    return bounds;
}

void eek_section_set_bounds(EekSection *section, EekBounds bounds) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    squeek_row_set_bounds(priv->row, bounds);
}

EekBounds eek_section_get_bounds(EekSection *section) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    return squeek_row_get_bounds(priv->row);
}

void
eek_section_place_keys(EekSection *section, LevelKeyboard *keyboard)
{
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    EekBounds section_size = squeek_row_place_keys(priv->row, keyboard);
    EekBounds section_bounds = eek_section_get_bounds(section);
    // FIXME: do centering of each section based on keyboard dimensions,
    // one level up the iterators
    // now centering by comparing previous width to the new, calculated one
    section_bounds.x = (section_bounds.width - section_size.width) / 2;
    section_bounds.width = section_size.width;
    section_bounds.height = section_size.height;
    eek_section_set_bounds(section, section_bounds);
}

void eek_section_foreach (EekSection *section,
                          ButtonCallback func,
                          gpointer   user_data) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    squeek_row_foreach(priv->row, func, user_data);
}

gboolean eek_section_find(EekSection *section,
                          struct squeek_button *button) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    return squeek_row_contains(priv->row, button) != 0;
}

struct squeek_button *eek_section_find_key(EekSection *section,
                                           struct squeek_key *key) {
    EekSectionPrivate *priv = eek_section_get_instance_private (section);
    return squeek_row_find_key(priv->row, key);
}
