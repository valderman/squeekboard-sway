/* 
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2019 Purism SPC
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
#include <xkbcommon/xkbcommon.h>

#include "config.h"

#include "eek/eek.h"
#include "eek/eek-xml-layout.h"

static void
test_check_xkb (void)
{
    EekLayout *layout;
    EekKeyboard *keyboard;
    GError *error;

    error = NULL;
    layout = eek_xml_layout_new ("us", &error);
    g_assert_no_error (error);

    keyboard = eek_keyboard_new (NULL, layout, 640, 480);
    gchar *keymap_str = eek_keyboard_get_keymap(keyboard);

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context) {
        g_error("No context created");
    }

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(context, keymap_str,
        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    free(keymap_str);

    xkb_context_unref(context);
    if (!keymap) {
        g_error("Bad keymap");
    }

    g_object_unref (layout);
    g_object_unref (keyboard);
}

int
main (int argc, char **argv)
{
    gtk_test_init (&argc, &argv, NULL);

    g_test_add_func ("/test-keymap-generation/check-xkb", test_check_xkb);

    return g_test_run ();
}
