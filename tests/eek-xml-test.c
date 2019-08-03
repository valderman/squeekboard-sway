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

/* For gdk_x11_display_get_xdisplay().  See main(). */
#include <gtk/gtk.h>

#include "config.h"

#include "eek/eek.h"
#include "eek/eek-xml-layout.h"

static void
test_output_parse (void)
{
    EekLayout *layout;
    LevelKeyboard *keyboard;
    GError *error;

    error = NULL;
    layout = eek_xml_layout_new ("us", &error);
    g_assert_no_error (error);

    /* We don't need the context service to parse an XML file, so we can pass
       NULL when creating a keyboard. */
    keyboard = eek_xml_layout_real_create_keyboard(layout, NULL);
    g_object_unref (layout);
    level_keyboard_free(keyboard);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/eek-xml-test/output-parse", test_output_parse);

    return g_test_run ();
}
