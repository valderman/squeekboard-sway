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
 * SECTION:eek-layout
 * @short_description: Base class of a layout engine
 *
 * The #EekLayout class is a base class of layout engine which
 * arranges keyboard elements.
 */

#include "config.h"

#include "eek-layout.h"
#include "eek-keyboard.h"
#include "eekboard/eekboard-context-service.h"

G_DEFINE_ABSTRACT_TYPE (EekLayout, eek_layout, G_TYPE_OBJECT)

static void
eek_layout_class_init (EekLayoutClass *klass)
{
    klass->create_keyboard = NULL;
}

void
eek_layout_init (EekLayout *self)
{
}

/**
 * eek_keyboard_new:
 * @layout: an #EekLayout
 * @initial_width: initial width of the keyboard
 * @initial_height: initial height of the keyboard
 *
 * Create a new #EekKeyboard based on @layout.
 */
EekKeyboard *
eek_keyboard_new (EekboardContextService *manager,
                  EekLayout *layout,
                  gdouble    initial_width,
                  gdouble    initial_height)
{
    g_assert (EEK_IS_LAYOUT(layout));
    g_assert (EEK_LAYOUT_GET_CLASS(layout)->create_keyboard);

    return EEK_LAYOUT_GET_CLASS(layout)->create_keyboard (manager,
                                                          layout,
                                                          initial_width,
                                                          initial_height);
}

const double section_spacing = 7.0;

struct place_data {
    double desired_width;
    double current_offset;
    EekKeyboard *keyboard;
};

static void
section_placer(EekElement *element, gpointer user_data)
{
    struct place_data *data = (struct place_data*)user_data;

    EekBounds section_bounds = {0};
    eek_element_get_bounds(element, &section_bounds);
    section_bounds.width = data->desired_width;
    eek_element_set_bounds(element, &section_bounds);

    // Sections are rows now. Gather up all the keys and adjust their bounds.
    eek_section_place_keys(EEK_SECTION(element), EEK_KEYBOARD(data->keyboard));

    eek_element_get_bounds(element, &section_bounds);
    section_bounds.y = data->current_offset;
    eek_element_set_bounds(element, &section_bounds);
    data->current_offset += section_bounds.height + section_spacing;
}

static void
section_counter(EekElement *element, gpointer user_data) {

    double *total_height = user_data;
    EekBounds section_bounds = {0};
    eek_element_get_bounds(element, &section_bounds);
    *total_height += section_bounds.height + section_spacing;
}

void
eek_layout_place_sections(EekKeyboard *keyboard)
{
    /* Order rows */
    // This needs to be done after outlines, because outlines define key sizes
    // TODO: do this only for rows without bounds

    // The keyboard width is given by the user via screen size. The height will be given dynamically.
    // TODO: calculate max line width beforehand for button centering. Leave keyboard centering to the renderer later
    EekBounds keyboard_bounds = {0};
    eek_element_get_bounds(EEK_ELEMENT(keyboard), &keyboard_bounds);

    struct place_data placer_data = {
        .desired_width = keyboard_bounds.width,
        .current_offset = 0,
        .keyboard = keyboard,
    };
    eek_container_foreach_child(EEK_CONTAINER(keyboard), section_placer, &placer_data);

    double total_height = 0;
    eek_container_foreach_child(EEK_CONTAINER(keyboard), section_counter, &total_height);
    keyboard_bounds.height = total_height;
    eek_element_set_bounds(EEK_ELEMENT(keyboard), &keyboard_bounds);
}

static void scale_bounds_callback (EekElement *element,
                                   gpointer    user_data);

static void
scale_bounds (EekElement *element,
              gdouble     scale)
{
    EekBounds bounds;

    eek_element_get_bounds (element, &bounds);
    bounds.x *= scale;
    bounds.y *= scale;
    bounds.width *= scale;
    bounds.height *= scale;
    eek_element_set_bounds (element, &bounds);

    if (EEK_IS_CONTAINER(element))
        eek_container_foreach_child (EEK_CONTAINER(element),
                                     scale_bounds_callback,
                                     &scale);
}

static void
scale_bounds_callback (EekElement *element,
                       gpointer    user_data)
{
    scale_bounds (element, *(gdouble *)user_data);
}

void
eek_layout_scale_keyboard(EekKeyboard *keyboard, gdouble scale)
{
    gsize n_outlines;

    scale_bounds (EEK_ELEMENT(keyboard), scale);

    n_outlines = eek_keyboard_get_n_outlines (keyboard);
    for (guint i = 0; i < n_outlines; i++) {
        EekOutline *outline = eek_keyboard_get_outline (keyboard, i);
        gint j;

        for (j = 0; j < outline->num_points; j++) {
            outline->points[j].x *= scale;
            outline->points[j].y *= scale;
        }
    }

    keyboard->scale = scale;
}

void
eek_layout_update_layout(EekKeyboard *keyboard)
{
    eek_layout_scale_keyboard(keyboard, 1.0 / keyboard->scale);
    eek_layout_place_sections(keyboard);
    eek_layout_scale_keyboard(keyboard, 1.0 / keyboard->scale);
}
