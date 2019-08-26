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
#include "eek-xml-layout.h"

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

const double row_spacing = 7.0;

struct place_data {
    double desired_width;
    double current_offset;
    // Needed for outline (bounds) retrieval
    LevelKeyboard *keyboard;
};

static void
row_placer(struct squeek_row *row, gpointer user_data)
{
    struct place_data *data = (struct place_data*)user_data;

    EekBounds row_bounds = {
        .x = 0,
        .y = 0,
        .width = data->desired_width,
        .height = 0,
    };
    squeek_row_set_bounds(row, row_bounds);

    // Gather up all the keys in a row and adjust their bounds.
    squeek_row_place_buttons(row, data->keyboard);

    row_bounds = squeek_row_get_bounds(row);
    row_bounds.y = data->current_offset;
    squeek_row_set_bounds(row, row_bounds);
    data->current_offset += row_bounds.height + row_spacing;
}

static void
row_counter(struct squeek_row *row, gpointer user_data) {

    double *total_height = user_data;
    EekBounds row_bounds = squeek_row_get_bounds(row);
    *total_height += row_bounds.height + row_spacing;
}

void
eek_layout_place_rows(LevelKeyboard *keyboard, struct squeek_view *level)
{
    /* Order rows */
    // This needs to be done after outlines, because outlines define key sizes
    // TODO: do this only for rows without bounds

    // The keyboard width is given by the user via screen size. The height will be given dynamically.
    // TODO: calculate max line width beforehand for button centering. Leave keyboard centering to the renderer later
    EekBounds view_bounds = squeek_view_get_bounds(level);

    struct place_data placer_data = {
        .desired_width = view_bounds.width,
        .current_offset = 0,
        .keyboard = keyboard,
    };
    squeek_view_foreach(level, row_placer, &placer_data);

    double total_height = 0;
    squeek_view_foreach(level, row_counter, &total_height);
    view_bounds.height = total_height;
    squeek_view_set_bounds(level, view_bounds);
}

void
eek_layout_update_layout(LevelKeyboard *keyboard)
{
    eek_layout_place_rows(keyboard, level_keyboard_current(keyboard));
}
