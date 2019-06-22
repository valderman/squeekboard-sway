/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2019 Purism, SPC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Modified for squeekboard based on GTK
 */

#include "keymap.h"

gboolean
squeek_keymap_get_entries_for_keyval (struct xkb_keymap     *xkb_keymap,
                                      guint          keyval,
                                      GdkKeymapKey **keys,
                                      guint          *n_keys)
{
  GArray *retval;
  guint keycode;
  xkb_keycode_t min_keycode, max_keycode;
  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  min_keycode = xkb_keymap_min_keycode (xkb_keymap);
  max_keycode = xkb_keymap_max_keycode (xkb_keymap);
  for (keycode = min_keycode; keycode < max_keycode; keycode++)
    {
      xkb_layout_index_t num_layouts, layout;
      num_layouts = xkb_keymap_num_layouts_for_key (xkb_keymap, keycode);
      for (layout = 0; layout < num_layouts; layout++)
        {
          xkb_layout_index_t num_levels, level;
          num_levels = xkb_keymap_num_levels_for_key (xkb_keymap, keycode, layout);
          for (level = 0; level < num_levels; level++)
            {
              const xkb_keysym_t *syms;
              gint num_syms, sym;
              num_syms = xkb_keymap_key_get_syms_by_level (xkb_keymap, keycode, layout, level, &syms);
              for (sym = 0; sym < num_syms; sym++)
                {
                  if (syms[sym] == keyval)
                    {
                      GdkKeymapKey key;
                      key.keycode = keycode;
                      key.group = (gint)layout;
                      key.level = (gint)level;
                      g_array_append_val (retval, key);
                    }
                }
            }
        }
    }
  *n_keys = retval->len;
  *keys = (GdkKeymapKey*) g_array_free (retval, FALSE);
  return TRUE;
}
