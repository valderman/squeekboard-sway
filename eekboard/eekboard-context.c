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

/**
 * SECTION:eekboard-context
 * @short_description: client interface of eekboard input context service
 *
 * The #EekboardContext class provides a client access to remote input
 * context.
 */

#include "config.h"

#include "eekboard/eekboard-context.h"
//#include "eekboard/eekboard-marshalers.h"

#define I_(string) g_intern_static_string (string)

enum {
    ENABLED,
    DISABLED,
    DESTROYED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

enum {
    PROP_0,
    PROP_VISIBLE,
    PROP_LAST
};

typedef struct _EekboardContextPrivate
{
    gboolean visible;
    gboolean enabled;
    gboolean fullscreen;
    gint group;
} EekboardContextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EekboardContext, eekboard_context, G_TYPE_DBUS_PROXY)

static void
eekboard_context_real_g_signal (GDBusProxy  *self,
                                const gchar *sender_name,
                                const gchar *signal_name,
                                GVariant    *parameters)
{
    EekboardContext *context = EEKBOARD_CONTEXT (self);
    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    if (g_strcmp0 (signal_name, "Enabled") == 0) {
        g_signal_emit (context, signals[ENABLED], 0);
        return;
    }

    if (g_strcmp0 (signal_name, "Disabled") == 0) {
        g_signal_emit (context, signals[DISABLED], 0);
        return;
    }

    if (g_strcmp0 (signal_name, "Destroyed") == 0) {
        g_signal_emit (context, signals[DESTROYED], 0);
        return;
    }

    if (g_strcmp0 (signal_name, "VisibilityChanged") == 0) {
        gboolean visible = FALSE;

        g_variant_get (parameters, "(b)", &visible);
        if (visible != priv->visible) {
            priv->visible = visible;
            g_object_notify (G_OBJECT(context), "visible");
        }
        return;
    }

    if (g_strcmp0 (signal_name, "GroupChanged") == 0) {
        gint group = 0;

        g_variant_get (parameters, "(i)", &group);
        if (group != priv->group) {
            priv->group = group;
            /* g_object_notify (G_OBJECT(context), "group"); */
        }
        return;
    }

    g_return_if_reached ();
}

static void
eekboard_context_real_enabled (EekboardContext *self)
{
    EekboardContextPrivate *priv = eekboard_context_get_instance_private (self);

    priv->enabled = TRUE;
}

static void
eekboard_context_real_disabled (EekboardContext *self)
{
    EekboardContextPrivate *priv = eekboard_context_get_instance_private (self);

    priv->enabled = FALSE;
}

static void
eekboard_context_real_destroyed (EekboardContext *self)
{
}

static void
eekboard_context_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    EekboardContext *context = EEKBOARD_CONTEXT(object);
    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    switch (prop_id) {
    case PROP_VISIBLE:
        g_value_set_boolean (value, priv->visible);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eekboard_context_class_init (EekboardContextClass *klass)
{
    GDBusProxyClass *proxy_class = G_DBUS_PROXY_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    klass->enabled = eekboard_context_real_enabled;
    klass->disabled = eekboard_context_real_disabled;
    klass->destroyed = eekboard_context_real_destroyed;

    proxy_class->g_signal = eekboard_context_real_g_signal;

    gobject_class->get_property = eekboard_context_get_property;

    /**
     * EekboardContext:visible:
     *
     * Flag to indicate if keyboard is visible or not.
     */
    pspec = g_param_spec_boolean ("visible",
                                  "visible",
                                  "Flag that indicates if keyboard is visible",
                                  FALSE,
                                  G_PARAM_READABLE);
    g_object_class_install_property (gobject_class,
                                     PROP_VISIBLE,
                                     pspec);

    /**
     * EekboardContext::enabled:
     * @context: an #EekboardContext
     *
     * Emitted when @context is enabled.
     */
    signals[ENABLED] =
        g_signal_new (I_("enabled"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekboardContextClass, enabled),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    /**
     * EekboardContext::disabled:
     * @context: an #EekboardContext
     *
     * The ::disabled signal is emitted each time @context is disabled.
     */
    signals[DISABLED] =
        g_signal_new (I_("disabled"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekboardContextClass, disabled),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    /**
     * EekboardContext::destroyed:
     * @context: an #EekboardContext
     *
     * The ::destroyed signal is emitted each time the name of remote
     * end is vanished.
     */
    signals[DESTROYED] =
        g_signal_new (I_("destroyed"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekboardContextClass, destroyed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
}

static void
eekboard_context_init (EekboardContext *self)
{
    /* void */
}

static void
context_name_vanished_callback (GDBusConnection *connection,
                                const gchar     *name,
                                gpointer         user_data)
{
    EekboardContext *context = user_data;
    g_signal_emit (context, signals[DESTROYED], 0);
}

/**
 * eekboard_context_new:
 * @connection: a #GDBusConnection
 * @object_path: object path
 * @cancellable: a #GCancellable
 *
 * Create a D-Bus proxy of an input context maintained by
 * eekboard-server.  This function is seldom called from applications
 * since eekboard_server_create_context() calls it implicitly.
 */
EekboardContext *
eekboard_context_new (GDBusConnection *connection,
                      const gchar     *object_path,
                      GCancellable    *cancellable)
{
    GInitable *initable;
    GError *error;

    g_return_val_if_fail (object_path != NULL, NULL);
    g_return_val_if_fail (G_IS_DBUS_CONNECTION(connection), NULL);

    error = NULL;
    initable =
        g_initable_new (EEKBOARD_TYPE_CONTEXT,
                        cancellable,
                        &error,
                        "g-name", "org.fedorahosted.Eekboard",
                        "g-connection", connection,
                        "g-interface-name", "org.fedorahosted.Eekboard.Context",
                        "g-object-path", object_path,
                        NULL);
    if (initable != NULL) {
        EekboardContext *context = EEKBOARD_CONTEXT (initable);
        gchar *name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY(context));

        if (name_owner == NULL) {
            g_object_unref (context);
            return NULL;
        }

        /* the vanished callback is called when the server is disconnected */
        g_bus_watch_name_on_connection (connection,
                                        name_owner,
                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
                                        NULL,
                                        context_name_vanished_callback,
                                        context,
                                        NULL);
        g_free (name_owner);

        return context;
    }

    g_warning ("can't create context client: %s", error->message);
    g_error_free (error);

    return NULL;
}

static void
context_async_ready_callback (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    GError *error = NULL;
    GVariant *result;

    result = g_dbus_proxy_call_finish (G_DBUS_PROXY(source_object),
                                       res,
                                       &error);
    if (result)
        g_variant_unref (result);
    else {
        g_warning ("error in D-Bus proxy call: %s", error->message);
        g_error_free (error);
    }
}

/**
 * eekboard_context_add_keyboard:
 * @context: an #EekboardContext
 * @keyboard: a string representing keyboard
 * @cancellable: a #GCancellable
 *
 * Register @keyboard in @context.
 */
guint
eekboard_context_add_keyboard (EekboardContext *context,
                               const gchar     *keyboard,
                               GCancellable    *cancellable)
{
    GVariant *result;
    GError *error;

    g_return_val_if_fail (EEKBOARD_IS_CONTEXT(context), 0);

    error = NULL;
    result = g_dbus_proxy_call_sync (G_DBUS_PROXY(context),
                                     "AddKeyboard",
                                     g_variant_new ("(s)", keyboard),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     cancellable,
                                     &error);

    if (result) {
        guint keyboard_id;

        g_variant_get (result, "(u)", &keyboard_id);
        g_variant_unref (result);

        return keyboard_id;
    }

    g_warning ("error in AddKeyboard call: %s", error->message);
    g_error_free (error);

    return 0;
}

/**
 * eekboard_context_remove_keyboard:
 * @context: an #EekboardContext
 * @keyboard_id: keyboard ID
 * @cancellable: a #GCancellable
 *
 * Unregister the keyboard with @keyboard_id in @context.
 */
void
eekboard_context_remove_keyboard (EekboardContext *context,
                                  guint            keyboard_id,
                                  GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    g_dbus_proxy_call (G_DBUS_PROXY(context),
                       "RemoveKeyboard",
                       g_variant_new ("(u)", keyboard_id),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       cancellable,
                       context_async_ready_callback,
                       NULL);
}

/**
 * eekboard_context_set_keyboard:
 * @context: an #EekboardContext
 * @keyboard_id: keyboard ID
 * @cancellable: a #GCancellable
 *
 * Select a keyboard with ID @keyboard_id in @context.
 */
void
eekboard_context_set_keyboard (EekboardContext *context,
                               guint            keyboard_id,
                               GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    g_dbus_proxy_call (G_DBUS_PROXY(context),
                       "SetKeyboard",
                       g_variant_new ("(u)", keyboard_id),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       cancellable,
                       context_async_ready_callback,
                       NULL);
}

/**
 * eekboard_context_set_group:
 * @context: an #EekboardContext
 * @group: group number
 * @cancellable: a #GCancellable
 *
 * Set the keyboard group of @context.
 */
void
eekboard_context_set_group (EekboardContext *context,
                            gint             group,
                            GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    if (priv->group != group) {
        g_dbus_proxy_call (G_DBUS_PROXY(context),
                           "SetGroup",
                           g_variant_new ("(i)", group),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           context_async_ready_callback,
                           NULL);
    }
}

/**
 * eekboard_context_get_group:
 * @context: an #EekboardContext
 * @cancellable: a #GCancellable
 *
 * Get the keyboard group of @context.
 */
gint
eekboard_context_get_group (EekboardContext *context,
                            GCancellable    *cancellable)
{
    g_return_val_if_fail (EEKBOARD_IS_CONTEXT(context), 0);

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    return priv->group;
}

/**
 * eekboard_context_show_keyboard:
 * @context: an #EekboardContext
 * @cancellable: a #GCancellable
 *
 * Request eekboard-server to show a keyboard set by
 * eekboard_context_set_keyboard().
 */
void
eekboard_context_show_keyboard (EekboardContext *context,
                                GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    if (priv->enabled) {
        g_dbus_proxy_call (G_DBUS_PROXY(context),
                           "ShowKeyboard",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           context_async_ready_callback,
                           NULL);
    }
}

/**
 * eekboard_context_hide_keyboard:
 * @context: an #EekboardContext
 * @cancellable: a #GCancellable
 *
 * Request eekboard-server to hide a keyboard.
 */
void
eekboard_context_hide_keyboard (EekboardContext *context,
                                GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    if (priv->enabled) {
        g_dbus_proxy_call (G_DBUS_PROXY(context),
                           "HideKeyboard",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           context_async_ready_callback,
                           NULL);
    }
}

/**
 * eekboard_context_press_keycode:
 * @context: an #EekboardContext
 * @keycode: keycode number
 * @cancellable: a #GCancellable
 *
 * Tell eekboard-server that a key identified by @keycode is pressed.
 */
void
eekboard_context_press_keycode (EekboardContext *context,
                                guint            keycode,
                                GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    if (priv->enabled) {
        g_dbus_proxy_call (G_DBUS_PROXY(context),
                           "PressKeycode",
                           g_variant_new ("(u)", keycode),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           context_async_ready_callback,
                           NULL);
    }
}

/**
 * eekboard_context_release_keycode:
 * @context: an #EekboardContext
 * @keycode: keycode number
 * @cancellable: a #GCancellable
 *
 * Tell eekboard-server that a key identified by @keycode is released.
 */
void
eekboard_context_release_keycode (EekboardContext *context,
                                  guint            keycode,
                                  GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    if (priv->enabled) {
        g_dbus_proxy_call (G_DBUS_PROXY(context),
                           "ReleaseKeycode",
                           g_variant_new ("(u)", keycode),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           context_async_ready_callback,
                           NULL);
    }
}

/**
 * eekboard_context_is_visible:
 * @context: an #EekboardContext
 *
 * Check if keyboard is visible.
 */
gboolean
eekboard_context_is_visible (EekboardContext *context)
{
    g_return_val_if_fail (EEKBOARD_IS_CONTEXT(context), FALSE);

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    return priv->enabled && priv->visible;
}

/**
 * eekboard_context_set_enabled:
 * @context: an #EekboardContext
 * @enabled: flag to indicate if @context is enabled
 *
 * Set @context enabled or disabled.  This function is seldom called
 * since the flag is set via D-Bus signal #EekboardContext::enabled
 * and #EekboardContext::disabled.
 */
void
eekboard_context_set_enabled (EekboardContext *context,
                              gboolean         enabled)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    priv->enabled = enabled;
}

/**
 * eekboard_context_is_enabled:
 * @context: an #EekboardContext
 *
 * Check if @context is enabled.
 */
gboolean
eekboard_context_is_enabled (EekboardContext *context)
{
    g_return_val_if_fail (EEKBOARD_IS_CONTEXT(context), FALSE);

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    return priv->enabled;
}

/**
 * eekboard_context_set_fullscreen:
 * @context: an #EekboardContext
 * @fullscreen: a flag to indicate fullscreen mode
 * @cancellable: a #GCancellable
 *
 * Set the fullscreen mode of @context.
 */
void
eekboard_context_set_fullscreen (EekboardContext *context,
                                 gboolean         fullscreen,
                                 GCancellable    *cancellable)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT(context));

    EekboardContextPrivate *priv = eekboard_context_get_instance_private (context);

    if (priv->fullscreen != fullscreen) {
        g_dbus_proxy_call (G_DBUS_PROXY(context),
                           "SetFullscreen",
                           g_variant_new ("(b)", fullscreen),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           context_async_ready_callback,
                           NULL);
    }
}
