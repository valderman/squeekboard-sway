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

#include "eek-keyboard.h"

void level_keyboard_deinit(LevelKeyboard *self) {
    xkb_keymap_unref(self->keymap);
    close(self->keymap_fd);
    squeek_layout_free(self->layout);
}

void level_keyboard_free(LevelKeyboard *self) {
    level_keyboard_deinit(self);
    g_free(self);
}

void level_keyboard_init(LevelKeyboard *self, struct squeek_layout *layout) {
    self->layout = layout;
}

LevelKeyboard *level_keyboard_new(struct squeek_layout *layout) {
    LevelKeyboard *keyboard = g_new0(LevelKeyboard, 1);
    level_keyboard_init(keyboard, layout);
    return keyboard;
}
