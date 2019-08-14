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
#include "eek-key.h"

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
} EekSectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekSection, eek_section, EEK_TYPE_CONTAINER)

static EekKey *
eek_section_real_create_key (EekSection *self,
                             const gchar *name,
                             gint        keycode,
                             guint oref)
{
    EekKey *key = (EekKey*)g_object_new (EEK_TYPE_KEY,
                                         "name", name,
                                         NULL);
    g_return_val_if_fail (key, NULL);
    eek_key_set_keycode(key, keycode);
    eek_key_set_oref(key, oref);

    EEK_CONTAINER_GET_CLASS(self)->add_child (EEK_CONTAINER(self),
                                              EEK_ELEMENT(key));

    return key;
}

EekKey *eek_section_create_button(EekSection *self,
                                  const gchar *name,
                                    struct squeek_key *state) {
    EekKey *key = (EekKey*)g_object_new (EEK_TYPE_KEY,
                                         "name", name,
                                         NULL);
    g_return_val_if_fail (key, NULL);
    eek_key_share_state(key, state);

    EEK_CONTAINER_GET_CLASS(self)->add_child (EEK_CONTAINER(self),
                                              EEK_ELEMENT(key));
    return key;
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
eek_section_real_child_added (EekContainer *self,
                              EekElement   *element)
{
    (void)self;
    (void)element;
}

static void
eek_section_real_child_removed (EekContainer *self,
                                EekElement   *element)
{
    (void)self;
    (void)element;
}

static void
eek_section_class_init (EekSectionClass *klass)
{
    EekContainerClass *container_class = EEK_CONTAINER_CLASS (klass);
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    klass->create_key = eek_section_real_create_key;

    /* signals */
    container_class->child_added = eek_section_real_child_added;
    container_class->child_removed = eek_section_real_child_removed;

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
    /* void */
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

/**
 * eek_section_create_key:
 * @section: an #EekSection
 * @name: a name
 * @keycode: a keycode
 * @column: the column index of the key
 * @row: the row index of the key
 *
 * Create an #EekKey instance and append it to @section.  This
 * function is rarely called by application but called by #EekLayout
 * implementation.
 */
EekKey *
eek_section_create_key (EekSection *section,
                        const gchar *name,
                        guint        keycode,
                        guint oref)
{
    g_return_val_if_fail (EEK_IS_SECTION(section), NULL);
    return eek_section_real_create_key (section,
                                                       name,
                                                       keycode, oref);
}

const double keyspacing = 4.0;

struct keys_info {
    uint count;
    double total_width;
    double biggest_height;
};

static void
keysizer(EekElement *element, gpointer user_data)
{
    EekKey *key = EEK_KEY(element);

    LevelKeyboard *keyboard = user_data;
    uint oref = eek_key_get_oref (key);
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
        EekBounds key_bounds = {0};
        eek_element_get_bounds(element, &key_bounds);
        key_bounds.height = maxy - miny;
        key_bounds.width = maxx - minx;
        eek_element_set_bounds(element, &key_bounds);
    }
}

static void
keycounter (EekElement *element, gpointer user_data)
{
    struct keys_info *data = user_data;
    data->count++;
    EekBounds key_bounds = {0};
    eek_element_get_bounds(element, &key_bounds);
    data->total_width += key_bounds.width;
    if (key_bounds.height > data->biggest_height) {
        data->biggest_height = key_bounds.height;
    }
}

static void
keyplacer(EekElement *element, gpointer user_data)
{
    double *current_offset = user_data;
    EekBounds key_bounds = {0};
    eek_element_get_bounds(element, &key_bounds);
    key_bounds.x = *current_offset;
    key_bounds.y = 0;
    eek_element_set_bounds(element, &key_bounds);
    *current_offset += key_bounds.width + keyspacing;
}

void
eek_section_place_keys(EekSection *section, LevelKeyboard *keyboard)
{
    eek_container_foreach_child(EEK_CONTAINER(section), keysizer, keyboard);

    struct keys_info keyinfo = {0};
    eek_container_foreach_child(EEK_CONTAINER(section), keycounter, &keyinfo);
    EekBounds section_bounds = {0};
    eek_element_get_bounds(EEK_ELEMENT(section), &section_bounds);

    double key_offset = (section_bounds.width - (keyinfo.total_width + (keyinfo.count - 1) * keyspacing)) / 2;
    eek_container_foreach_child(EEK_CONTAINER(section), keyplacer, &key_offset);

    section_bounds.height = keyinfo.biggest_height;
    eek_element_set_bounds(EEK_ELEMENT(section), &section_bounds);
}
