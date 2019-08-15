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
#include "eek/eek.h"

#include <gtk/gtk.h>

static void
test_create (void)
{
    EekKeyboard *keyboard;
    struct squeek_button *button0, *button1;

    keyboard = g_object_new (EEK_TYPE_KEYBOARD, NULL);
    struct squeek_row *row = eek_keyboard_real_create_row (keyboard);
    g_assert (row);
    button0 = squeek_row_create_button (row, 1, 0);
    g_assert (button0);
    button1 = squeek_row_create_button (row, 2, 0);
    g_assert (button1);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/eek-simple-test/create", test_create);
    return g_test_run ();
}
