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

#include "config.h"

#include "eek-section.h"

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

void
eek_row_place_buttons(struct squeek_row *row, LevelKeyboard *keyboard)
{
    EekBounds row_size = squeek_row_place_keys(row, keyboard);
    EekBounds row_bounds = squeek_row_get_bounds(row);
    // FIXME: do centering of each row based on keyboard dimensions,
    // one level up the iterators
    // now centering by comparing previous width to the new, calculated one
    row_bounds.x = (row_bounds.width - row_size.width) / 2;
    row_bounds.width = row_size.width;
    row_bounds.height = row_size.height;
    squeek_row_set_bounds(row, row_bounds);
}
