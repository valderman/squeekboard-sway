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

#include "config.h"

#include "eek/eek.h"
#include "eekboard/eekboard-context-service.h"
#include "dbus.h"
#include "imservice.h"
#include "outputs.h"
#include "server-context-service.h"
#include "wayland.h"

#include <gdk/gdkwayland.h>


/// Global application state
struct squeekboard {
    struct squeek_wayland wayland;
    ServerContextService *context;
    struct imservice *imservice;
};


static gboolean opt_system = FALSE;
static gchar *opt_address = NULL;

// D-Bus

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    (void)connection;
    (void)name;
    (void)user_data;
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
    // TODO: could conceivable continue working
    (void)connection;
    (void)name;
    (void)user_data;
    exit (1);
}

static ServerContextService *create_context() {
    ServerContextService *context = server_context_service_new ();
    g_object_set_data_full (G_OBJECT(context),
                            "owner", g_strdup ("sender"),
                            (GDestroyNotify)g_free);
    return context;
}

// Wayland

static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t name,
                        const char *interface,
                        uint32_t version)
{
    // currently only v1 supported for most interfaces,
    // so there's no reason to check for available versions.
    // Even when lower version would be served, it would not be supported,
    // causing a hard exit
    (void)version;
    struct squeekboard *instance = data;

    if (!strcmp (interface, zwlr_layer_shell_v1_interface.name)) {
        instance->wayland.layer_shell = wl_registry_bind (registry, name,
            &zwlr_layer_shell_v1_interface, 1);
    } else if (!strcmp (interface, zwp_virtual_keyboard_manager_v1_interface.name)) {
        instance->wayland.virtual_keyboard_manager = wl_registry_bind(registry, name,
            &zwp_virtual_keyboard_manager_v1_interface, 1);
    } else if (!strcmp (interface, zwp_input_method_manager_v2_interface.name)) {
        instance->wayland.input_method_manager = wl_registry_bind(registry, name,
            &zwp_input_method_manager_v2_interface, 1);
    } else if (!strcmp (interface, "wl_output")) {
        struct wl_output *output = wl_registry_bind (registry, name,
            &wl_output_interface, 2);
        squeek_outputs_register(instance->wayland.outputs, output);
    } else if (!strcmp(interface, "wl_seat")) {
        instance->wayland.seat = wl_registry_bind(registry, name,
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

#define SESSION_NAME "sm.puri.OSK0"

GDBusProxy *_proxy = NULL;

static void
session_register(void) {
    char *autostart_id = getenv("DESKTOP_AUTOSTART_ID");
    if (!autostart_id) {
        g_debug("No autostart id");
        autostart_id = "";
    }
    GError *error = NULL;
    _proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, NULL,
        "org.gnome.SessionManager", "/org/gnome/SessionManager",
        "org.gnome.SessionManager", NULL, &error);
    if (error) {
        g_warning("Could not connect to session manager: %s\n",
                error->message);
        g_clear_error(&error);
        return;
    }

    g_dbus_proxy_call_sync(_proxy, "RegisterClient",
        g_variant_new("(ss)", SESSION_NAME, autostart_id),
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    if (error) {
        g_warning("Could not register to session manager: %s\n",
                error->message);
        g_clear_error(&error);
        return;
    }
}

int
main (int argc, char **argv)
{
    if (!gtk_init_check (&argc, &argv)) {
        g_printerr ("Can't init GTK\n");
        exit (1);
    }

    eek_init ();

    // Set up Wayland
    gdk_set_allowed_backends ("wayland");
    GdkDisplay *gdk_display = gdk_display_get_default ();
    struct wl_display *display = gdk_wayland_display_get_wl_display (gdk_display);

    if (display == NULL) {
        g_error ("Failed to get display: %m\n");
        exit(1);
    }


    struct squeekboard instance = {0};
    squeek_wayland_init (&instance.wayland);
    struct wl_registry *registry = wl_display_get_registry (display);
    wl_registry_add_listener (registry, &registry_listener, &instance);
    wl_display_roundtrip(display); // wait until the registry is actually populated
    squeek_wayland_set_global(&instance.wayland);

    if (!instance.wayland.seat) {
        g_error("No seat Wayland global available.");
        exit(1);
    }
    if (!instance.wayland.virtual_keyboard_manager) {
        g_error("No virtual keyboard manager Wayland global available.");
        exit(1);
    }

    instance.context = create_context();

    // set up dbus

    // TODO: make dbus errors non-always-fatal
    // dbus is not strictly necessary for the useful operation
    // if text-input is used, as it can bring the keyboard in and out
    GBusType bus_type;
    if (opt_system)
        bus_type = G_BUS_TYPE_SYSTEM;
    else if (opt_address)
        bus_type = G_BUS_TYPE_NONE;
    else
        bus_type = G_BUS_TYPE_SESSION;

    GDBusConnection *connection = NULL;
    GError *error = NULL;
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

    DBusHandler *service = dbus_handler_new(connection, DBUS_SERVICE_PATH);

    if (service == NULL) {
        g_printerr ("Can't create dbus server\n");
        exit (1);
    } else {
        dbus_handler_set_context(service, instance.context);
    }

    guint owner_id = g_bus_own_name_on_connection (connection,
                                             DBUS_SERVICE_INTERFACE,
                                             G_BUS_NAME_OWNER_FLAGS_NONE,
                                             on_name_acquired,
                                             on_name_lost,
                                             NULL,
                                             NULL);
    if (owner_id == 0) {
        g_printerr ("Can't own the name\n");
        exit (1);
    }

    struct imservice *imservice = NULL;
    if (instance.wayland.input_method_manager) {
        imservice = get_imservice(instance.context,
                                  instance.wayland.input_method_manager,
                                  instance.wayland.seat);
        if (imservice) {
            instance.imservice = imservice;
        } else {
            g_warning("Failed to register as an input method");
        }
    }

    session_register();

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    g_main_loop_run (loop);

    g_bus_unown_name (owner_id);
    g_object_unref (service);
    g_object_unref (connection);
    g_main_loop_unref (loop);

    squeek_wayland_deinit (&instance.wayland);
    return 0;
}
