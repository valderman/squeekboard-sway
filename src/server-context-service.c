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
#include "eekboard/eekboard-context-service.h"
#include "submission.h"
#include "wayland.h"
#include "server-context-service.h"

enum {
    PROP_0,
    PROP_SIZE_CONSTRAINT_LANDSCAPE,
    PROP_SIZE_CONSTRAINT_PORTRAIT,
    PROP_VISIBLE,
    PROP_LAST
};

typedef struct _ServerContextServiceClass ServerContextServiceClass;

struct _ServerContextService {
    GObject parent;

    EekboardContextService *state; // unowned
    /// Needed for instantiating the widget
    struct submission *submission; // unowned

    gboolean visible;
    PhoshLayerSurface *window;
    GtkWidget *widget; // nullable
    guint hiding;
    guint last_requested_height;
    enum squeek_arrangement_kind last_type;

    gdouble size_constraint_landscape[2];
    gdouble size_constraint_portrait[2];
};

struct _ServerContextServiceClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE(ServerContextService, server_context_service, G_TYPE_OBJECT);

static void
on_destroy (GtkWidget *widget, gpointer user_data)
{
    ServerContextService *context = user_data;

    g_assert (widget == GTK_WIDGET(context->window));

    context->window = NULL;
    context->widget = NULL;

    //eekboard_context_service_destroy (EEKBOARD_CONTEXT_SERVICE (context));
}

static void
make_widget (ServerContextService *context);

static void
on_notify_keyboard (GObject              *object,
                    GParamSpec           *spec,
                    ServerContextService *context)
{
    /* Recreate the keyboard widget to keep in sync with the keymap. */
    if (context->window)
        make_widget(context);

    gboolean visible;
    g_object_get (context, "visible", &visible, NULL);

    if (visible) {
        server_context_service_hide_keyboard(context);
        server_context_service_show_keyboard(context);
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
    (void)object;
    g_object_set (context, "visible", FALSE, NULL);
}

static uint32_t
calculate_height(int32_t width)
{
    uint32_t height = 180;
    if (width < 360 && width > 0) {
        height = ((unsigned)width * 7 / 12); // to match 360×210
    } else if (width < 540) {
        height = 180 + (540 - (unsigned)width) * 30 / 180; // smooth transition
    }
    return height;
}

enum squeek_arrangement_kind get_type(uint32_t width, uint32_t height) {
    (void)height;
    if (width < 540) {
        return ARRANGEMENT_KIND_BASE;
    }
    return ARRANGEMENT_KIND_WIDE;
}

static void
on_surface_configure(PhoshLayerSurface *surface, ServerContextService *context)
{
    gint width;
    gint height;
    g_object_get(G_OBJECT(surface),
                 "configured-width", &width,
                 "configured-height", &height,
                 NULL);
    // check if the change would switch types
    enum squeek_arrangement_kind new_type = get_type((uint32_t)width, (uint32_t)height);
    if (context->last_type != new_type) {
        context->last_type = new_type;
        eekboard_context_service_update_layout(context->state, context->last_type);
    }

    guint desired_height = calculate_height(width);
    guint configured_height = (guint)height;
    // if height was already requested once but a different one was given
    // (for the same set of surrounding properties),
    // then it's probably not reasonable to ask for it again,
    // as it's likely to create pointless loops
    // of request->reject->request_again->...
    if (desired_height != configured_height
            && context->last_requested_height != desired_height) {
        context->last_requested_height = desired_height;
        phosh_layer_surface_set_size(surface, 0,
                                     (gint)desired_height);
        phosh_layer_surface_set_exclusive_zone(surface, (gint)desired_height);
        phosh_layer_surface_wl_surface_commit (surface);
    }
}

static void
make_window (ServerContextService *context)
{
    if (context->window)
        g_error("Window already present");

    struct wl_output *output = squeek_outputs_get_current(squeek_wayland->outputs);
    int32_t width = squeek_outputs_get_perceptual_width(squeek_wayland->outputs, output);
    uint32_t height = calculate_height(width);

    context->window = g_object_new (
        PHOSH_TYPE_LAYER_SURFACE,
        "layer-shell", squeek_wayland->layer_shell,
        "wl-output", output,
        "height", height,
        "anchor", ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
                  | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
                  | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        "layer", ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "kbd-interactivity", FALSE,
        "exclusive-zone", height,
        "namespace", "osk",
        NULL
    );

    g_object_connect (context->window,
                      "signal::destroy", G_CALLBACK(on_destroy), context,
                      "signal::map", G_CALLBACK(on_notify_map), context,
                      "signal::unmap", G_CALLBACK(on_notify_unmap), context,
                      "signal::configured", G_CALLBACK(on_surface_configure), context,
                      NULL);

    // The properties below are just to make hacking easier.
    // The way we use layer-shell overrides some,
    // and there's no space in the protocol for others.
    // Those may still be useful in the future,
    // or for hacks with regular windows.
    gtk_widget_set_can_focus (GTK_WIDGET(context->window), FALSE);
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

    LevelKeyboard *keyboard = eekboard_context_service_get_keyboard (context->state);

    context->widget = eek_gtk_keyboard_new (keyboard, context->state, context->submission);

    gtk_widget_set_has_tooltip (context->widget, TRUE);
    gtk_container_add (GTK_CONTAINER(context->window), context->widget);
    gtk_widget_show (context->widget);
}

static gboolean
on_hide (ServerContextService *context)
{
    gtk_widget_hide (GTK_WIDGET(context->window));
    context->hiding = 0;

    return G_SOURCE_REMOVE;
}

static void
server_context_service_real_show_keyboard (ServerContextService *context)
{
    if (context->hiding) {
	    g_source_remove (context->hiding);
	    context->hiding = 0;
    }

    if (!context->window)
        make_window (context);
    if (!context->widget)
        make_widget (context);

    context->visible = TRUE;
    gtk_widget_show (GTK_WIDGET(context->window));
}

static void
server_context_service_real_hide_keyboard (ServerContextService *context)
{
    if (!context->hiding)
        context->hiding = g_timeout_add (200, (GSourceFunc) on_hide, context);

    context->visible = FALSE;
}

void
server_context_service_show_keyboard (ServerContextService *context)
{
    g_return_if_fail (SERVER_IS_CONTEXT_SERVICE(context));

    if (!context->visible) {
        server_context_service_real_show_keyboard (context);
    }
}

void
server_context_service_hide_keyboard (ServerContextService *context)
{
    g_return_if_fail (SERVER_IS_CONTEXT_SERVICE(context));

    if (context->visible) {
        server_context_service_real_hide_keyboard (context);
    }
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
    case PROP_VISIBLE:
        context->visible = g_value_get_boolean (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
server_context_service_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    ServerContextService *context = SERVER_CONTEXT_SERVICE(object);
    switch (prop_id) {
    case PROP_VISIBLE:
        g_value_set_boolean (value, context->visible);
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
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->set_property = server_context_service_set_property;
    gobject_class->get_property = server_context_service_get_property;
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

    /**
     * Flag to indicate if keyboard is visible or not.
     */
    pspec = g_param_spec_boolean ("visible",
                                  "Visible",
                                  "Visible",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_VISIBLE,
                                     pspec);
}

static void
server_context_service_init (ServerContextService *state) {
    (void)state;
}

ServerContextService *
server_context_service_new (EekboardContextService *state, struct submission *submission)
{
    ServerContextService *ui = g_object_new (SERVER_TYPE_CONTEXT_SERVICE, NULL);
    ui->submission = submission;
    ui->state = state;
    g_signal_connect (state,
                      "notify::keyboard",
                      G_CALLBACK(on_notify_keyboard),
                      ui);
    return ui;
}

enum squeek_arrangement_kind server_context_service_get_layout_type(ServerContextService *service)
{
    return service->last_type;
}
