/* 
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#if !defined(__EEKBOARD_CLIENT_H_INSIDE__) && !defined(EEKBOARD_COMPILATION)
#error "Only <eekboard/eekboard-client.h> can be included directly."
#endif

#ifndef EEKBOARD_CONTEXT_H
#define EEKBOARD_CONTEXT_H 1

#include <gio/gio.h>
#include "eek/eek.h"

G_BEGIN_DECLS

#define EEKBOARD_TYPE_CONTEXT (eekboard_context_get_type())
G_DECLARE_DERIVABLE_TYPE (EekboardContext, eekboard_context, EEKBOARD, CONTEXT, GDBusProxy)

/**
 * EekboardContextClass:
 * @enabled: class handler for #EekboardContext::enabled signal
 * @disabled: class handler for #EekboardContext::disabled signal
 * @key_pressed: class handler for #EekboardContext::key-pressed signal
 * @destroyed: class handler for #EekboardContext::destroyed signal
 */
struct _EekboardContextClass {
    /*< private >*/
    GDBusProxyClass parent_class;

    /*< public >*/
    /* signals */
    void (*enabled)       (EekboardContext *self);
    void (*disabled)      (EekboardContext *self);
    void (*destroyed)     (EekboardContext *self);

    void (*key_activated) (EekboardContext *self,
                           guint            keycode,
                           EekSymbol       *symbol,
                           guint            modifiers);

    /*< private >*/
    /* padding */
    gpointer pdummy[24];
};

GType            eekboard_context_get_type       (void) G_GNUC_CONST;

EekboardContext *eekboard_context_new            (GDBusConnection *connection,
                                                  const gchar     *object_path,
                                                  GCancellable    *cancellable);
guint            eekboard_context_add_keyboard   (EekboardContext *context,
                                                  const gchar     *keyboard,
                                                  GCancellable    *cancellable);
void             eekboard_context_remove_keyboard (EekboardContext *context,
                                                   guint            keyboard_id,
                                                   GCancellable    *cancellable);
void             eekboard_context_set_keyboard   (EekboardContext *context,
                                                  guint            keyboard_id,
                                                  GCancellable    *cancellable);
void             eekboard_context_show_keyboard  (EekboardContext *context,
                                                  GCancellable    *cancellable);
void             eekboard_context_hide_keyboard  (EekboardContext *context,
                                                  GCancellable    *cancellable);
void             eekboard_context_set_group      (EekboardContext *context,
                                                  gint             group,
                                                  GCancellable    *cancellable);
gint             eekboard_context_get_group      (EekboardContext *context,
                                                  GCancellable    *cancellable);
void             eekboard_context_press_keycode  (EekboardContext *context,
                                                  guint            keycode,
                                                  GCancellable    *cancellable);
void             eekboard_context_release_keycode (EekboardContext *context,
                                                   guint            keycode,
                                                   GCancellable    *cancellable);
gboolean         eekboard_context_is_visible
                                                 (EekboardContext *context);
void             eekboard_context_set_enabled    (EekboardContext *context,
                                                  gboolean         enabled);
gboolean         eekboard_context_is_enabled     (EekboardContext *context);
void             eekboard_context_set_fullscreen (EekboardContext *context,
                                                  gboolean         fullscreen,
                                                  GCancellable    *cancellable);

G_END_DECLS
#endif  /* EEKBOARD_CONTEXT_H */
