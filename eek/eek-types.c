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
 * SECTION:eek-types
 * @title: Miscellaneous Types
 * @short_description: Miscellaneous types used in Libeek
 */

#include "config.h"

#include <string.h>
#include <math.h>

#include "eek-types.h"

/* EekPoint */
G_DEFINE_BOXED_TYPE(EekPoint, eek_point, eek_point_copy, eek_point_free);

EekPoint *
eek_point_copy (const EekPoint *point)
{
    return g_slice_dup (EekPoint, point);
}

void
eek_point_free (EekPoint *point)
{
    g_slice_free (EekPoint, point);
}

void
eek_point_rotate (EekPoint *point, gint angle)
{
    gdouble r, phi;

    phi = atan2 (point->y, point->x);
    r = sqrt (point->x * point->x + point->y * point->y);
    phi += angle * M_PI / 180;
    point->x = r * cos (phi);
    point->y = r * sin (phi);
}

/* EekBounds */
G_DEFINE_BOXED_TYPE(EekBounds, eek_bounds, eek_bounds_copy, eek_bounds_free);

EekBounds *
eek_bounds_copy (const EekBounds *bounds)
{
    return g_slice_dup (EekBounds, bounds);
}

void
eek_bounds_free (EekBounds *bounds)
{
    g_slice_free (EekBounds, bounds);
}
