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

#include <gdk/gdkx.h>

#include "eek/eek-gtk.h"
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

    gdouble size_constraint_landscape[2];
    gdouble size_constraint_portrait[2];
};

struct _ServerContextServiceClass {
    EekboardContextServiceClass parent_class;
};

G_DEFINE_TYPE (ServerContextService, server_context_service, EEKBOARD_TYPE_CONTEXT_SERVICE);

static void set_geometry  (ServerContextService *context);

static void
on_monitors_changed (GdkScreen            *screen,
                     ServerContextService *context)

{
    if (context->window)
        set_geometry (context);
}

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
on_notify_keyboard (GObject              *object,
                    GParamSpec           *spec,
                    ServerContextService *context)
{
    const EekKeyboard *keyboard;

    keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));

    if (!keyboard)
        g_error("Programmer error: keyboard layout was unset!");

    // The keymap will get set even if the window is hidden.
    // It's not perfect,
    // but simpler than adding a check in the window showing procedure
    eekboard_context_service_set_keymap(EEKBOARD_CONTEXT_SERVICE(context),
                                        keyboard);

    /* Recreate the keyboard widget to keep in sync with the keymap. */
    gboolean visible;
    g_object_get (context, "visible", &visible, NULL);

    if (visible) {
        eekboard_context_service_hide_keyboard(EEKBOARD_CONTEXT_SERVICE(context));
        eekboard_context_service_show_keyboard(EEKBOARD_CONTEXT_SERVICE(context));
    }
}

static void
on_notify_fullscreen (GObject              *object,
                      GParamSpec           *spec,
                      ServerContextService *context)
{
    if (context->window)
        set_geometry (context);
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


static void
set_geometry (ServerContextService *context)
{
    GdkScreen   *screen   = gdk_screen_get_default ();
    GdkWindow   *root     = gdk_screen_get_root_window (screen);
    GdkDisplay  *display  = gdk_display_get_default ();
    GdkMonitor  *monitor  = gdk_display_get_monitor_at_window (display, root);
    EekKeyboard *keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));

    GdkRectangle rect;
    EekBounds bounds;

    gdk_monitor_get_geometry (monitor, &rect);
    eek_element_get_bounds (EEK_ELEMENT(keyboard), &bounds);

    if (eekboard_context_service_get_fullscreen (EEKBOARD_CONTEXT_SERVICE(context))) {
        gint width  = rect.width;
        gint height = rect.height;

        if (width > height) {
            width  *= context->size_constraint_landscape[0];
            height *= context->size_constraint_landscape[1];
        } else {
            width  *= context->size_constraint_portrait[0];
            height *= context->size_constraint_portrait[1];
        }

        if (width * bounds.height > height * bounds.width)
            width = (height / bounds.height) * bounds.width;
        else
            height = (width / bounds.width) * bounds.height;

        gtk_window_resize (GTK_WINDOW(context->widget), width, height);

        gtk_window_move (GTK_WINDOW(context->window),
                         (rect.width - width) / 2,
                         rect.height - height);

        gtk_window_set_decorated (GTK_WINDOW(context->window), FALSE);
        gtk_window_set_resizable (GTK_WINDOW(context->window), FALSE);
    } else {
        gtk_window_resize (GTK_WINDOW(context->window),
                           bounds.width,
                           bounds.height);
        gtk_window_move (GTK_WINDOW(context->window),
                         MAX(rect.width - 20 - bounds.width, 0),
                         MAX(rect.height - 40 - bounds.height, 0));
    }
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
    EekKeyboard *keyboard;
    EekTheme *theme;

    g_return_if_fail (!context->widget);
    theme = eek_theme_new ("resource:///sm/puri/squeekboard/style.css",
                           NULL,
                           NULL);

    keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));

    context->widget = eek_gtk_keyboard_new (keyboard);

    eek_gtk_keyboard_set_theme (EEK_GTK_KEYBOARD(context->widget), theme);
    g_clear_object (&theme);

    gtk_widget_set_has_tooltip (context->widget, TRUE);
    gtk_container_add (GTK_CONTAINER(context->window), context->widget);
    gtk_widget_show (context->widget);
    set_geometry (context);
}

static void
server_context_service_real_show_keyboard (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    if (!context->window)
        make_window (context);
    if (context->widget) {
        gtk_widget_destroy(context->widget);
        context->widget = NULL;
    }

    make_widget (context);

    EEKBOARD_CONTEXT_SERVICE_CLASS (server_context_service_parent_class)->
        show_keyboard (_context);
    gtk_widget_show (context->window);
}

static void
server_context_service_real_hide_keyboard (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    gtk_widget_hide (context->window);

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
    GdkScreen *screen = gdk_screen_get_default ();

    g_signal_connect (screen,
                      "monitors-changed",
                      G_CALLBACK(on_monitors_changed),
                      context);
    g_signal_connect (context,
                      "notify::keyboard",
                      G_CALLBACK(on_notify_keyboard),
                      context);

    g_signal_connect (context,
                      "notify::fullscreen",
                      G_CALLBACK(on_notify_fullscreen),
                      context);
}

EekboardContextService *
server_context_service_new ()
{
    return EEKBOARD_CONTEXT_SERVICE(g_object_new (SERVER_TYPE_CONTEXT_SERVICE, NULL));
}
