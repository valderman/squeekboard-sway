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

#ifndef EEK_GTK_RENDERER_H
#define EEK_GTK_RENDERER_H 1

#include <gtk/gtk.h>
#include "eek-renderer.h"

G_BEGIN_DECLS

#define EEK_TYPE_GTK_RENDERER (eek_gtk_renderer_get_type())
G_DECLARE_FINAL_TYPE (EekGtkRenderer, eek_gtk_renderer, EEK, GTK_RENDERER, EekRenderer)

GType        eek_gtk_renderer_get_type (void) G_GNUC_CONST;
EekRenderer *eek_gtk_renderer_new      (EekKeyboard  *keyboard,
                                        PangoContext *pcontext,
                                        GtkWidget *widget);

G_END_DECLS
#endif  /* EEK_GTK_RENDERER_H */
