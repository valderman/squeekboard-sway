/* 
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2018-2019 Purism SPC
 * SPDX-License-Identifier: GPL-3.0+
 * Author: Guido Günther <agx@sigxcpu.org>
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
#include <stdlib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#if HAVE_CLUTTER_GTK
#include <clutter-gtk/clutter-gtk.h>
#endif

#include "server-service.h"
#include "eek/eek.h"
#include "wayland.h"

#include <gdk/gdkwayland.h>

static gboolean opt_system = FALSE;
static gchar *opt_address = NULL;

// D-Bus

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  exit (1);
}

static void
on_destroyed (ServerService *service,
              gpointer      user_data)
{
    GMainLoop *loop = user_data;

    g_main_loop_quit (loop);
}

// Wayland

static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t name,
                        const char *interface,
                        uint32_t version)
{
    struct squeak_wayland *wayland = data;

    if (!strcmp (interface, zwlr_layer_shell_v1_interface.name)) {
        wayland->layer_shell = wl_registry_bind (registry, name,
            &zwlr_layer_shell_v1_interface, 1);
    } else if (!strcmp (interface, "wl_output")) {
        struct wl_output *output = wl_registry_bind (registry, name,
            &wl_output_interface, 2);
        g_ptr_array_add (wayland->outputs, output);
    } else if (!strcmp(interface, "wl_seat")) {
        wayland->seat = wl_registry_bind(registry, name,
            &wl_seat_interface, 1);
    }
}


static void
registry_handle_global_remove (void *data,
                               struct wl_registry *registry,
                               uint32_t name)
{
  // TODO
}

static const struct wl_registry_listener registry_listener = {
  registry_handle_global,
  registry_handle_global_remove
};

int
main (int argc, char **argv)
{
    ServerService *service;
    GBusType bus_type;
    GDBusConnection *connection;
    GError *error;
    GMainLoop *loop;
    guint owner_id;

#if HAVE_CLUTTER_GTK
    if (gtk_clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS) {
        g_printerr ("Can't init GTK with Clutter\n");
        exit (1);
    }
#else
    if (!gtk_init_check (&argc, &argv)) {
        g_printerr ("Can't init GTK\n");
        exit (1);
    }
#endif

    eek_init ();

    if (opt_system)
        bus_type = G_BUS_TYPE_SYSTEM;
    else if (opt_address)
        bus_type = G_BUS_TYPE_NONE;
    else
        bus_type = G_BUS_TYPE_SESSION;

    switch (bus_type) {
    case G_BUS_TYPE_SYSTEM:
    case G_BUS_TYPE_SESSION:
        error = NULL;
        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
        if (connection == NULL) {
            g_printerr ("Can't connect to the bus: %s\n", error->message);
            g_error_free (error);
            exit (1);
        }
        break;
    case G_BUS_TYPE_NONE:
        error = NULL;
        connection = g_dbus_connection_new_for_address_sync (opt_address,
                                                             0,
                                                             NULL,
                                                             NULL,
                                                             &error);
        if (connection == NULL) {
            g_printerr ("Can't connect to the bus at %s: %s\n",
                        opt_address,
                        error->message);
            g_error_free (error);
            exit (1);
        }
        break;
    default:
        g_assert_not_reached ();
        break;
    }

    // Set up Wayland
    gdk_set_allowed_backends ("wayland");
    GdkDisplay *gdk_display = gdk_display_get_default ();
    struct wl_display *display = gdk_wayland_display_get_wl_display (gdk_display);

    if (display == NULL) {
        g_error ("Failed to get display: %m\n");
    }

    struct squeak_wayland wayland;
    squeak_wayland_init (&wayland);
    struct wl_registry *registry = wl_display_get_registry (display);
    wl_registry_add_listener (registry, &registry_listener, &wayland);
    squeak_wayland_set_global(&wayland);
    service = server_service_new (EEKBOARD_SERVICE_PATH, connection);

    if (service == NULL) {
        g_printerr ("Can't create server\n");
        exit (1);
    }

    owner_id = g_bus_own_name_on_connection (connection,
                                             EEKBOARD_SERVICE_INTERFACE,
                                             G_BUS_NAME_OWNER_FLAGS_NONE,
                                             on_name_acquired,
                                             on_name_lost,
                                             NULL,
                                             NULL);
    if (owner_id == 0) {
        g_printerr ("Can't own the name\n");
        exit (1);
    }

    loop = g_main_loop_new (NULL, FALSE);

    g_signal_connect (service, "destroyed", G_CALLBACK(on_destroyed), loop);

    g_main_loop_run (loop);

    g_bus_unown_name (owner_id);
    g_object_unref (service);
    g_object_unref (connection);
    g_main_loop_unref (loop);

    squeak_wayland_deinit (&wayland);
    return 0;
}
