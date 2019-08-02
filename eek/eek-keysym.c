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
 * SECTION:eek-keysym
 * @short_description: an #EekSymbol represents an X keysym
 */

#include "config.h"

#include "eek-keysym.h"

/* modifier keys */
#define EEK_KEYSYM_Shift_L 0xffe1
#define EEK_KEYSYM_Shift_R 0xffe2
#define EEK_KEYSYM_ISO_Level3_Shift 0xfe03
#define EEK_KEYSYM_Caps_Lock 0xffe5
#define EEK_KEYSYM_Shift_Lock 0xffe6
#define EEK_KEYSYM_Control_L 0xffe3
#define EEK_KEYSYM_Control_R 0xffe4
#define EEK_KEYSYM_Alt_L 0xffe9
#define EEK_KEYSYM_Alt_R 0xffea
#define EEK_KEYSYM_Meta_L 0xffe7
#define EEK_KEYSYM_Meta_R 0xffe8
#define EEK_KEYSYM_Super_L 0xffeb
#define EEK_KEYSYM_Super_R 0xffec
#define EEK_KEYSYM_Hyper_L 0xffed
#define EEK_KEYSYM_Hyper_R 0xffee

struct _EekKeysymEntry {
    guint xkeysym;
    const gchar *name;
};

typedef struct _EekKeysymEntry EekKeysymEntry;

#include "eek-special-keysym-entries.h"
#include "eek-unicode-keysym-entries.h"
#include "eek-xkeysym-keysym-entries.h"

guint32
eek_keysym_from_name (const gchar *name)
{
    for (uint i = 0; i < G_N_ELEMENTS(xkeysym_keysym_entries); i++) {
        if (g_strcmp0 (xkeysym_keysym_entries[i].name, name) == 0) {
            return xkeysym_keysym_entries[i].xkeysym;
        }
    }
    return 0;
}
