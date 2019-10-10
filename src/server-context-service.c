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
#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "eek/eek.h"
#include "eek/eek-gtk-keyboard.h"
#include "eek/layersurface.h"
#include "wayland.h"

#include "server-context-service.h"

enum {
    PROP_0,
    PROP_SIZE_CONSTRAINT_LANDSCAPE,
    PROP_SIZE_CONSTRAINT_PORTRAIT,
    PROP_LAST
};

typedef struct _ServerContextServiceClass ServerContextServiceClass;

struct _ServerContextService {
    EekboardContextService parent;

    GtkWidget *window;
    GtkWidget *widget;
    guint hiding;

    gdouble size_constraint_landscape[2];
    gdouble size_constraint_portrait[2];
};

struct _ServerContextServiceClass {
    EekboardContextServiceClass parent_class;
};

G_DEFINE_TYPE (ServerContextService, server_context_service, EEKBOARD_TYPE_CONTEXT_SERVICE);

static void
on_destroy (GtkWidget *widget, gpointer user_data)
{
    ServerContextService *context = user_data;

    g_assert (widget == context->window);

    context->window = NULL;
    context->widget = NULL;

    eekboard_context_service_destroy (EEKBOARD_CONTEXT_SERVICE (context));
}

static void
make_widget (ServerContextService *context);

static void
on_notify_keyboard (GObject              *object,
                    GParamSpec           *spec,
                    ServerContextService *context)
{
    const LevelKeyboard *keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));

    if (!keyboard)
        g_error("Programmer error: keyboard layout was unset!");

    // The keymap will get set even if the window is hidden.
    // It's not perfect,
    // but simpler than adding a check in the window showing procedure
    eekboard_context_service_set_keymap(EEKBOARD_CONTEXT_SERVICE(context),
                                        keyboard);

    /* Recreate the keyboard widget to keep in sync with the keymap. */
    if (context->window)
        make_widget(context);

    gboolean visible;
    g_object_get (context, "visible", &visible, NULL);

    if (visible) {
        eekboard_context_service_hide_keyboard(EEKBOARD_CONTEXT_SERVICE(context));
        eekboard_context_service_show_keyboard(EEKBOARD_CONTEXT_SERVICE(context));
    }
}

static void
on_notify_map (GObject    *object,
               ServerContextService *context)
{
    g_object_set (context, "visible", TRUE, NULL);
}


static void
on_notify_unmap (GObject    *object,
                 ServerContextService *context)
{
    g_object_set (context, "visible", FALSE, NULL);
}

#define KEYBOARD_HEIGHT 210
static void
make_window (ServerContextService *context)
{
    if (context->window)
        g_error("Window already present");

    context->window = g_object_new (
        PHOSH_TYPE_LAYER_SURFACE,
        "layer-shell", squeek_wayland->layer_shell,
        "wl-output", g_ptr_array_index(squeek_wayland->outputs, 0), // TODO: select output as needed,
        "height", KEYBOARD_HEIGHT,
        "anchor", ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
                  | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
                  | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        "layer", ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "kbd-interactivity", FALSE,
        "exclusive-zone", KEYBOARD_HEIGHT,
        "namespace", "osk",
        NULL
    );

    g_object_connect (context->window,
                      "signal::destroy", G_CALLBACK(on_destroy), context,
                      "signal::map", G_CALLBACK(on_notify_map), context,
                      "signal::unmap", G_CALLBACK(on_notify_unmap), context,
                      NULL);

    // The properties below are just to make hacking easier.
    // The way we use layer-shell overrides some,
    // and there's no space in the protocol for others.
    // Those may still be useful in the future,
    // or for hacks with regular windows.
    gtk_widget_set_can_focus (context->window, FALSE);
    g_object_set (G_OBJECT(context->window), "accept_focus", FALSE, NULL);
    gtk_window_set_title (GTK_WINDOW(context->window),
                          _("Squeekboard"));
    gtk_window_set_icon_name (GTK_WINDOW(context->window), "squeekboard");
    gtk_window_set_keep_above (GTK_WINDOW(context->window), TRUE);
}

static void
destroy_window (ServerContextService *context)
{
    gtk_widget_destroy (GTK_WIDGET (context->window));
    context->window = NULL;
}

static void
make_widget (ServerContextService *context)
{
    if (context->widget) {
        gtk_widget_destroy(context->widget);
        context->widget = NULL;
    }

    LevelKeyboard *keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));

    context->widget = eek_gtk_keyboard_new (keyboard);

    gtk_widget_set_has_tooltip (context->widget, TRUE);
    gtk_container_add (GTK_CONTAINER(context->window), context->widget);
    gtk_widget_show (context->widget);
}

static void
server_context_service_real_show_keyboard (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    if (context->hiding) {
	    g_source_remove (context->hiding);
	    context->hiding = 0;
    }

    if (!context->window)
        make_window (context);
    if (!context->widget)
        make_widget (context);

    EEKBOARD_CONTEXT_SERVICE_CLASS (server_context_service_parent_class)->
        show_keyboard (_context);
    gtk_widget_show (context->window);
}

static gboolean
on_hide (ServerContextService *context)
{
    gtk_widget_hide (context->window);
    context->hiding = 0;

    return G_SOURCE_REMOVE;
}

static void
server_context_service_real_hide_keyboard (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    if (!context->hiding)
	context->hiding = g_timeout_add (200, (GSourceFunc) on_hide, context);

    EEKBOARD_CONTEXT_SERVICE_CLASS (server_context_service_parent_class)->
        hide_keyboard (_context);
}

static void
server_context_service_real_destroyed (EekboardContextService *_context)
{
}

static void
server_context_service_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(object);
    GVariant *variant;

    switch (prop_id) {
    case PROP_SIZE_CONSTRAINT_LANDSCAPE:
        variant = g_value_get_variant (value);
        g_variant_get (variant, "(dd)",
                       &context->size_constraint_landscape[0],
                       &context->size_constraint_landscape[1]);
        break;
    case PROP_SIZE_CONSTRAINT_PORTRAIT:
        variant = g_value_get_variant (value);
        g_variant_get (variant, "(dd)",
                       &context->size_constraint_portrait[0],
                       &context->size_constraint_portrait[1]);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
server_context_service_dispose (GObject *object)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(object);

    destroy_window (context);
    context->widget = NULL;

    G_OBJECT_CLASS (server_context_service_parent_class)->dispose (object);
}

static void
server_context_service_class_init (ServerContextServiceClass *klass)
{
    EekboardContextServiceClass *context_class = EEKBOARD_CONTEXT_SERVICE_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    context_class->show_keyboard = server_context_service_real_show_keyboard;
    context_class->hide_keyboard = server_context_service_real_hide_keyboard;
    context_class->destroyed = server_context_service_real_destroyed;

    gobject_class->set_property = server_context_service_set_property;
    gobject_class->dispose = server_context_service_dispose;

    pspec = g_param_spec_variant ("size-constraint-landscape",
                                  "Size constraint landscape",
                                  "Size constraint landscape",
                                  G_VARIANT_TYPE("(dd)"),
                                  NULL,
                                  G_PARAM_WRITABLE);
    g_object_class_install_property (gobject_class,
                                     PROP_SIZE_CONSTRAINT_LANDSCAPE,
                                     pspec);

    pspec = g_param_spec_variant ("size-constraint-portrait",
                                  "Size constraint portrait",
                                  "Size constraint portrait",
                                  G_VARIANT_TYPE("(dd)"),
                                  NULL,
                                  G_PARAM_WRITABLE);
    g_object_class_install_property (gobject_class,
                                     PROP_SIZE_CONSTRAINT_PORTRAIT,
                                     pspec);
}

static void
server_context_service_init (ServerContextService *context)
{
    g_signal_connect (context,
                      "notify::keyboard",
                      G_CALLBACK(on_notify_keyboard),
                      context);
}

EekboardContextService *
server_context_service_new ()
{
    return EEKBOARD_CONTEXT_SERVICE(g_object_new (SERVER_TYPE_CONTEXT_SERVICE, NULL));
}
