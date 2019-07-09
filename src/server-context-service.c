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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

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

    gboolean was_visible;

    GtkWidget *window;
    GtkWidget *widget;

    gulong notify_visible_handler;

    gdouble size_constraint_landscape[2];
    gdouble size_constraint_portrait[2];
};

struct _ServerContextServiceClass {
    EekboardContextServiceClass parent_class;
};

G_DEFINE_TYPE (ServerContextService, server_context_service, EEKBOARD_TYPE_CONTEXT_SERVICE);

static void update_widget (ServerContextService *context);
static void set_geometry  (ServerContextService *context);
static void set_dock      (GtkWidget            *widget,
                           GtkAllocation        *allocation);

static void
on_monitors_changed (GdkScreen *screen,
                     gpointer   user_data)
{
    ServerContextService *context = user_data;
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
on_notify_keyboard (GObject    *object,
                    GParamSpec *spec,
                    gpointer    user_data)
{
    ServerContextService *context = user_data;
    const EekKeyboard *keyboard;

    keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));

    if (!keyboard) {
        g_error("Programmer error: keyboard layout was unset!");
    }

    // The keymap will get set even if the window is hidden.
    // It's not perfect,
    // but simpler than adding a check in the window showing procedure
    eekboard_context_service_set_keymap(EEKBOARD_CONTEXT_SERVICE(context),
                                        keyboard);

    if (context->window) {
        if (keyboard == NULL) {
            gtk_widget_hide (context->window);
            g_clear_pointer (&context->widget, gtk_widget_destroy);
        } else {
            gboolean was_visible = gtk_widget_get_visible (context->window);
            /* avoid to send KeyboardVisibilityChanged */
            g_signal_handler_block (context->window,
                                    context->notify_visible_handler);
            update_widget (context);
            if (was_visible) {
                gtk_widget_show_all (context->window);
            }
            g_signal_handler_unblock (context->window,
                                      context->notify_visible_handler);
        }
    }
}
        
static void
on_notify_fullscreen (GObject    *object,
                      GParamSpec *spec,
                      gpointer    user_data)
{
    ServerContextService *context = user_data;
    if (context->window)
        set_geometry (context);
}

static void
on_notify_visible (GObject *object, GParamSpec *spec, gpointer user_data)
{
    ServerContextService *context = user_data;
    gboolean visible;

    g_object_get (object, "visible", &visible, NULL);
    g_object_set (context, "visible", visible, NULL);
}

static void
set_dock (GtkWidget *widget, GtkAllocation *allocation)
{
#ifdef HAVE_XDOCK
    GdkWindow *window = gtk_widget_get_window (widget);
    long vals[12];

    /* set window type to dock */
    gdk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DOCK);
  
    vals[0] = 0;
    vals[1] = 0;
    vals[2] = 0;
    vals[3] = allocation->height;
    vals[4] = 0;
    vals[5] = 0;
    vals[6] = 0;
    vals[7] = 0;
    vals[8] = 0;
    vals[9] = 0;
    vals[10] = allocation->x;
    vals[11] = allocation->x + allocation->width;

    XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                     GDK_WINDOW_XID (window),
                     XInternAtom (GDK_WINDOW_XDISPLAY (window),
                                  "_NET_WM_STRUT_PARTIAL", False),
                     XA_CARDINAL, 32, PropModeReplace,
                     (guchar *)vals, 12);
#endif  /* HAVE_XDOCK */
}

static void
on_realize_set_dock (GtkWidget *widget,
                     gpointer   user_data)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);
    set_dock (widget, &allocation);
}

static void
on_size_allocate_set_dock (GtkWidget *widget,
                           GdkRectangle *allocation,
                           gpointer user_data)
{
    if (gtk_widget_get_realized (widget))
        set_dock (widget, allocation);
}

static void
on_realize_set_non_maximizable (GtkWidget *widget,
                                gpointer   user_data)
{
    ServerContextService *context = user_data;

    g_assert (context && context->window == widget);

    /* make the window not maximizable */
    gdk_window_set_functions (gtk_widget_get_window (widget),
                              GDK_FUNC_RESIZE |
                              GDK_FUNC_MOVE |
                              GDK_FUNC_MINIMIZE |
                              GDK_FUNC_CLOSE);
}

static void
set_geometry (ServerContextService *context)
{
    GdkScreen *screen;
    GdkDisplay *display;
    GdkWindow *root;
    GdkMonitor *monitor;
    GdkRectangle rect;
    EekKeyboard *keyboard;
    EekBounds bounds;

    screen = gdk_screen_get_default ();
    root = gdk_screen_get_root_window (screen);
    display = gdk_display_get_default();
    monitor = gdk_display_get_monitor_at_window (display, root);
    gdk_monitor_get_geometry (monitor, &rect);
    keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));
    eek_element_get_bounds (EEK_ELEMENT(keyboard), &bounds);

    g_signal_handlers_disconnect_by_func (context->window,
                                          on_realize_set_dock,
                                          context);
    g_signal_handlers_disconnect_by_func (context->window,
                                          on_realize_set_non_maximizable,
                                          context);

    if (eekboard_context_service_get_fullscreen (EEKBOARD_CONTEXT_SERVICE(context))) {
        gint width = rect.width;
        gint height = rect.height;

        if (width > height) {
            width *= context->size_constraint_landscape[0];
            height *= context->size_constraint_landscape[1];
        } else {
            width *= context->size_constraint_portrait[0];
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
        g_signal_connect_after (context->window, "realize",
                                G_CALLBACK(on_realize_set_dock),
                                context);
        g_signal_connect_after (context->window, "size-allocate",
                                G_CALLBACK(on_size_allocate_set_dock),
                                context);
    } else {
        gtk_window_resize (GTK_WINDOW(context->window),
                           bounds.width,
                           bounds.height);
        gtk_window_move (GTK_WINDOW(context->window),
                         MAX(rect.width - 20 - bounds.width, 0),
                         MAX(rect.height - 40 - bounds.height, 0));
        g_signal_connect_after (context->window, "realize",
                                G_CALLBACK(on_realize_set_non_maximizable),
                                context);
    }
}

static void
make_window (ServerContextService *context) {
    if (context->window) {
        g_error("Window already present");
        return;
    }
    context->window = GTK_WIDGET(g_object_new (
        PHOSH_TYPE_LAYER_SURFACE,
        "layer-shell", squeek_wayland->layer_shell,
        "wl-output", g_ptr_array_index(squeek_wayland->outputs, 0), // TODO: select output as needed,
        "height", 200,
        "anchor", ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
                  | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
                  | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        "layer", ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "kbd-interactivity", FALSE,
        "exclusive-zone", 200,
        //"namespace", "phosh home",
        NULL
    ));
    g_signal_connect (context->window, "destroy",
                      G_CALLBACK(on_destroy), context);
    context->notify_visible_handler =
        g_signal_connect (context->window, "notify::visible",
                          G_CALLBACK(on_notify_visible), context);

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
    context->window = NULL; // FIXME: doesn't destroy much
}

static void
update_widget (ServerContextService *context)
{
    EekKeyboard *keyboard;
    EekBounds bounds;
    gchar *theme_path;
    EekTheme *theme;
    
    g_clear_pointer (&context->widget, gtk_widget_destroy);

    theme_path = g_build_filename (THEMESDIR, "default.css", NULL);

    theme = eek_theme_new (theme_path, NULL, NULL);
    g_free (theme_path);

    keyboard = eekboard_context_service_get_keyboard (EEKBOARD_CONTEXT_SERVICE(context));
    eek_element_get_bounds (EEK_ELEMENT(keyboard), &bounds);
    context->widget = eek_gtk_keyboard_new (keyboard);
    eek_gtk_keyboard_set_theme (EEK_GTK_KEYBOARD(context->widget), theme);
    g_object_unref (theme);

    gtk_widget_set_has_tooltip (context->widget, TRUE);
    gtk_container_add (GTK_CONTAINER(context->window), context->widget);
    set_geometry (context);
}

static void
server_context_service_real_show_keyboard (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    make_window (context);
    update_widget (context);

    EEKBOARD_CONTEXT_SERVICE_CLASS (server_context_service_parent_class)->
        show_keyboard (_context);
}

static void
server_context_service_real_hide_keyboard (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    gtk_widget_hide (context->window);
    g_clear_pointer (&context->widget, gtk_widget_destroy);

    destroy_window (context); // <--- FIXME: looks suspect

    EEKBOARD_CONTEXT_SERVICE_CLASS (server_context_service_parent_class)->
        hide_keyboard (_context);
}

static void
server_context_service_real_enabled (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    if (context->was_visible && context->window)
        gtk_widget_show_all (context->window);
}

static void
server_context_service_real_disabled (EekboardContextService *_context)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(_context);

    if (context->window) {
        context->was_visible =
            gtk_widget_get_visible (context->window);
        gtk_widget_hide (context->window);
    }
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

    g_clear_pointer (&context->window, gtk_widget_destroy);
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
    context_class->enabled = server_context_service_real_enabled;
    context_class->disabled = server_context_service_real_disabled;
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
    GdkScreen *screen;

    screen = gdk_screen_get_default ();
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
    return EEKBOARD_CONTEXT_SERVICE(g_object_new (SERVER_TYPE_CONTEXT_SERVICE,
                         NULL));
}
