/* 
 * Copyright (C) 2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2011 Red Hat, Inc.
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

#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "eek-gtk-renderer.h"
#include "eek-key.h"

struct _EekGtkRenderer {
    EekRenderer parent;

    GHashTable *icons;
};

G_DEFINE_TYPE (EekGtkRenderer, eek_gtk_renderer, EEK_TYPE_RENDERER);

static cairo_surface_t *
eek_gtk_renderer_real_get_icon_surface (EekRenderer *renderer,
                                        const gchar *icon_name,
                                        gint size,
                                        gint scale)
{
    EekGtkRenderer *self = EEK_GTK_RENDERER (renderer);
    GError *error = NULL;
    cairo_surface_t *surface;

    surface = g_hash_table_lookup (self->icons, icon_name);
    if (!surface) {
        surface = gtk_icon_theme_load_surface (gtk_icon_theme_get_default (),
                                               icon_name,
                                               size,
                                               scale,
                                               NULL,
                                               0,
                                               &error);
        g_hash_table_insert (self->icons, g_strdup(icon_name), surface);
        if (surface == NULL) {
            g_warning ("can't get icon surface for %s: %s",
                       icon_name,
                       error->message);
            g_error_free (error);
            return NULL;
        }
    }
    return surface;
}

static void
eek_gtk_renderer_dispose (GObject *object)
{
  EekGtkRenderer *self = EEK_GTK_RENDERER (object);

  g_clear_pointer (&self->icons, g_hash_table_destroy);

  G_OBJECT_CLASS (eek_gtk_renderer_parent_class)->dispose (object);
}

static void
eek_gtk_renderer_class_init (EekGtkRendererClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;
    EekRendererClass *renderer_class = EEK_RENDERER_CLASS (klass);

    object_class->dispose = eek_gtk_renderer_dispose;

    renderer_class->get_icon_surface = eek_gtk_renderer_real_get_icon_surface;
}

static void
eek_gtk_renderer_init (EekGtkRenderer *self)
{
    GtkIconTheme *theme = gtk_icon_theme_get_default ();

    gtk_icon_theme_add_resource_path (theme, "/sm/puri/squeekboard/icons");
    self->icons = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         (GDestroyNotify)cairo_surface_destroy);
}

EekRenderer *
eek_gtk_renderer_new (EekKeyboard  *keyboard,
                      PangoContext *pcontext,
                      GtkWidget    *widget)
{
    return g_object_new (EEK_TYPE_GTK_RENDERER,
                         "keyboard", keyboard,
                         "pango-context", pcontext,
                         NULL);
}
