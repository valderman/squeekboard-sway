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
#include <libxklavier/xklavier.h>
#include <cspi/spi.h>
#include <gdk/gdkx.h>
#include <fakekey/fakekey.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include "eek/eek.h"
#include "eek/eek-xkl.h"
#include "system-client.h"
#include "proxy.h"

#define CSW 640
#define CSH 480

enum {
    PROP_0,
    PROP_CONNECTION,
    PROP_LAST
};

typedef struct _EekboardSystemClientClass EekboardSystemClientClass;

struct _EekboardSystemClient {
    GObject parent;

    EekboardProxy *proxy;

    EekKeyboard *keyboard;
    Accessible *accessible;
    XklEngine *xkl_engine;
    XklConfigRegistry *xkl_config_registry;
    FakeKey *fakekey;

    gulong xkl_config_changed_handler;
    gulong xkl_state_changed_handler;

    gulong key_pressed_handler;
    gulong key_released_handler;

    AccessibleEventListener *focus_listener;
    AccessibleEventListener *keystroke_listener;
};

struct _EekboardSystemClientClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE (EekboardSystemClient, eekboard_system_client, G_TYPE_OBJECT);

static GdkFilterReturn filter_xkl_event  (GdkXEvent                 *xev,
                                          GdkEvent                  *event,
                                          gpointer                   user_data);
static void            on_xkl_config_changed
                                         (XklEngine                 *xklengine,
                                          gpointer                   user_data);

static void            on_xkl_state_changed
                                         (XklEngine                 *xklengine,
                                          XklEngineStateChange       type,
                                          gint                       value,
                                          gboolean                   restore,
                                          gpointer                   user_data);

static SPIBoolean      focus_listener_cb (const AccessibleEvent     *event,
                                          void                      *user_data);

static SPIBoolean      keystroke_listener_cb
                                         (const AccessibleKeystroke *stroke,
                                          void                      *user_data);
static void            set_keyboard      (EekboardSystemClient     *client);

static void
eekboard_system_client_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
    EekboardSystemClient *client = EEKBOARD_SYSTEM_CLIENT(object);
    GDBusConnection *connection;
    GError *error;

    switch (prop_id) {
    case PROP_CONNECTION:
        connection = g_value_get_object (value);
        error = NULL;
        client->proxy = eekboard_proxy_new ("/com/redhat/eekboard/Keyboard",
                                             connection,
                                             NULL,
                                             &error);
        break;
    default:
        g_object_set_property (object,
                               g_param_spec_get_name (pspec),
                               value);
        break;
    }
}

static void
eekboard_system_client_dispose (GObject *object)
{
    EekboardSystemClient *client = EEKBOARD_SYSTEM_CLIENT(object);

    eekboard_system_client_disable_xkl (client);
    eekboard_system_client_disable_cspi (client);
    eekboard_system_client_disable_fakekey (client);

    if (client->proxy) {
        g_object_unref (client->proxy);
        client->proxy = NULL;
    }
    
    if (client->fakekey) {
        client->fakekey = NULL;
    }

    G_OBJECT_CLASS (eekboard_system_client_parent_class)->dispose (object);
}

static void
eekboard_system_client_class_init (EekboardSystemClientClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->set_property = eekboard_system_client_set_property;
    gobject_class->dispose = eekboard_system_client_dispose;

    pspec = g_param_spec_object ("connection",
                                 "Connection",
                                 "Connection",
                                 G_TYPE_DBUS_CONNECTION,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);
    g_object_class_install_property (gobject_class,
                                     PROP_CONNECTION,
                                     pspec);
}

static void
eekboard_system_client_init (EekboardSystemClient *client)
{
    client->keyboard = NULL;
    client->accessible = NULL;
    client->xkl_engine = NULL;
    client->xkl_config_registry = NULL;
    client->focus_listener = NULL;
    client->keystroke_listener = NULL;
    client->proxy = NULL;
    client->fakekey = NULL;
    client->key_pressed_handler = 0;
    client->key_released_handler = 0;
    client->xkl_config_changed_handler = 0;
    client->xkl_state_changed_handler = 0;
}

gboolean
eekboard_system_client_enable_xkl (EekboardSystemClient *client)
{
    if (!client->xkl_engine) {
        Display *display;

        display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        client->xkl_engine = xkl_engine_get_instance (display);
    }

    if (!client->xkl_config_registry) {
        client->xkl_config_registry =
            xkl_config_registry_get_instance (client->xkl_engine);
        xkl_config_registry_load (client->xkl_config_registry, FALSE);
    }

    client->xkl_config_changed_handler =
        g_signal_connect (client->xkl_engine, "X-config-changed",
                          G_CALLBACK(on_xkl_config_changed), client);
    client->xkl_state_changed_handler =
        g_signal_connect (client->xkl_engine, "X-state-changed",
                          G_CALLBACK(on_xkl_state_changed), client);

    gdk_window_add_filter (NULL,
                           (GdkFilterFunc) filter_xkl_event,
                           client);
    gdk_window_add_filter (gdk_get_default_root_window (),
                           (GdkFilterFunc) filter_xkl_event,
                           client);

    xkl_engine_start_listen (client->xkl_engine, XKLL_TRACK_KEYBOARD_STATE);

    set_keyboard (client);

    return TRUE;
}

void
eekboard_system_client_disable_xkl (EekboardSystemClient *client)
{
    if (client->xkl_engine)
        xkl_engine_stop_listen (client->xkl_engine, XKLL_TRACK_KEYBOARD_STATE);
    if (g_signal_handler_is_connected (client->xkl_engine,
                                       client->xkl_config_changed_handler))
        g_signal_handler_disconnect (client->xkl_engine,
                                     client->xkl_config_changed_handler);
    if (g_signal_handler_is_connected (client->xkl_engine,
                                       client->xkl_state_changed_handler))
        g_signal_handler_disconnect (client->xkl_engine,
                                     client->xkl_state_changed_handler);
}

gboolean
eekboard_system_client_enable_cspi (EekboardSystemClient *client)
{
    client->focus_listener = SPI_createAccessibleEventListener
        ((AccessibleEventListenerCB)focus_listener_cb,
         client);

    if (!SPI_registerGlobalEventListener (client->focus_listener,
                                          "object:state-changed:focused"))
        return FALSE;

    if (!SPI_registerGlobalEventListener (client->focus_listener,
                                          "focus:"))
        return FALSE;

    client->keystroke_listener =
        SPI_createAccessibleKeystrokeListener (keystroke_listener_cb,
                                               client);

    if (!SPI_registerAccessibleKeystrokeListener
        (client->keystroke_listener,
         SPI_KEYSET_ALL_KEYS,
         0,
         SPI_KEY_PRESSED |
         SPI_KEY_RELEASED,
         SPI_KEYLISTENER_NOSYNC))
        return FALSE;
    return TRUE;
}

void
eekboard_system_client_disable_cspi (EekboardSystemClient *client)
{
    if (client->focus_listener) {
        SPI_deregisterGlobalEventListenerAll (client->focus_listener);
        AccessibleEventListener_unref (client->focus_listener);
        client->focus_listener = NULL;
    }
    if (client->keystroke_listener) {
        SPI_deregisterAccessibleKeystrokeListener (client->keystroke_listener,
                                                   0);
        AccessibleKeystrokeListener_unref (client->keystroke_listener);
        client->keystroke_listener = NULL;
    }
}

EekboardSystemClient *
eekboard_system_client_new (GDBusConnection *connection)
{
    return g_object_new (EEKBOARD_TYPE_SYSTEM_CLIENT,
                         "connection", connection,
                         NULL);
}

static SPIBoolean
focus_listener_cb (const AccessibleEvent *event,
                   void                  *user_data)
{
    EekboardSystemClient *client = user_data;
    Accessible *accessible = event->source;
    AccessibleStateSet *state_set = Accessible_getStateSet (accessible);
    AccessibleRole role = Accessible_getRole (accessible);

    if (AccessibleStateSet_contains (state_set, SPI_STATE_EDITABLE) ||
        role == SPI_ROLE_TERMINAL) {
        switch (role) {
        case SPI_ROLE_TEXT:
        case SPI_ROLE_PARAGRAPH:
        case SPI_ROLE_PASSWORD_TEXT:
        case SPI_ROLE_TERMINAL:
            if (g_strcmp0 (event->type, "focus") == 0 ||
                event->detail1 == 1) {
                client->accessible = accessible;
                eekboard_proxy_show (client->proxy);
            } else if (event->detail1 == 0 &&
                       accessible == client->accessible) {
                client->accessible = NULL;
                eekboard_proxy_hide (client->proxy);
            }
        case SPI_ROLE_ENTRY:
            if (g_strcmp0 (event->type, "focus") == 0 ||
                event->detail1 == 1) {
                client->accessible = accessible;
                eekboard_proxy_show (client->proxy);
            } else if (event->detail1 == 0 &&
                       accessible == client->accessible) {
                client->accessible = NULL;
                eekboard_proxy_hide (client->proxy);
            }
        default:
            ;
        }
    }

    return FALSE;
}

static SPIBoolean
keystroke_listener_cb (const AccessibleKeystroke *stroke,
                       void                      *user_data)
{
    EekboardSystemClient *client = user_data;
    EekKey *key;
    EekSymbol *symbol;

    return FALSE;

    key = eek_keyboard_find_key_by_keycode (client->keyboard,
                                            stroke->keycode);
    if (!key)
        return FALSE;

    symbol = eek_key_get_symbol_with_fallback (key, 0, 0);
    if (!symbol)
        return FALSE;

    /* XXX: Ignore modifier keys since there is no way to receive
       SPI_KEY_RELEASED event for them. */
    if (eek_symbol_is_modifier (symbol))
        return FALSE;

    if (stroke->type == SPI_KEY_PRESSED)
        g_signal_emit_by_name (key, "pressed");
    else
        g_signal_emit_by_name (key, "released");
    return TRUE;
}

static GdkFilterReturn
filter_xkl_event (GdkXEvent *xev,
                  GdkEvent  *event,
                  gpointer   user_data)
{
    EekboardSystemClient *client = user_data;
    XEvent *xevent = (XEvent *)xev;

    xkl_engine_filter_events (client->xkl_engine, xevent);
    return GDK_FILTER_CONTINUE;
}

static void
on_xkl_config_changed (XklEngine *xklengine,
                       gpointer   user_data)
{
    EekboardSystemClient *client = user_data;

    set_keyboard (client);
}

static void
set_keyboard (EekboardSystemClient *client)
{
    EekLayout *layout;
    gchar *keyboard_name;
    static gint keyboard_serial = 0;

    if (client->keyboard)
        g_object_unref (client->keyboard);
    layout = eek_xkl_layout_new ();
    client->keyboard = eek_keyboard_new (layout, CSW, CSH);

    keyboard_name = g_strdup_printf ("keyboard%d", keyboard_serial++);
    eek_element_set_name (EEK_ELEMENT(client->keyboard), keyboard_name);

    eekboard_proxy_set_keyboard (client->proxy, client->keyboard);
}

static void
on_xkl_state_changed (XklEngine           *xklengine,
                      XklEngineStateChange type,
                      gint                 value,
                      gboolean             restore,
                      gpointer             user_data)
{
    EekboardSystemClient *client = user_data;

    if (type == GROUP_CHANGED && client->keyboard) {
        gint group = eek_keyboard_get_group (client->keyboard);
        if (group != value) {
            eek_keyboard_set_group (client->keyboard, value);
            eekboard_proxy_set_group (client->proxy, value);
        }
    }
}

static void
on_key_pressed (EekboardProxy *proxy,
                guint          keycode,
                gpointer       user_data)
{
    EekboardSystemClient *client = user_data;
    EekKey *key;
    EekSymbol *symbol;

    g_assert (client->fakekey);
    key = eek_keyboard_find_key_by_keycode (client->keyboard, keycode);
    symbol = eek_key_get_symbol_with_fallback (key, 0, 0);
    if (EEK_IS_KEYSYM(symbol) && !eek_symbol_is_modifier (symbol))
        fakekey_press_keysym (client->fakekey,
                              eek_keysym_get_xkeysym (EEK_KEYSYM(symbol)),
                              eek_keyboard_get_modifiers (client->keyboard));
}

static void
on_key_released (EekboardProxy *proxy,
                 guint          keycode,
                 gpointer       user_data)
{
    EekboardSystemClient *client = user_data;

    g_assert (client->fakekey);
    fakekey_release (client->fakekey);
}

gboolean
eekboard_system_client_enable_fakekey (EekboardSystemClient *client)
{
    if (!client->fakekey) {
        Display *display;

        display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        client->fakekey = fakekey_init (display);
    }
    g_assert (client->fakekey);

    client->key_pressed_handler =
        g_signal_connect (client->proxy, "key-pressed",
                          G_CALLBACK(on_key_pressed), client);
    client->key_released_handler =
        g_signal_connect (client->proxy, "key-pressed",
                          G_CALLBACK(on_key_released), client);

    return TRUE;
}

void
eekboard_system_client_disable_fakekey (EekboardSystemClient *client)
{
    if (client->fakekey)
        fakekey_release (client->fakekey);

    if (g_signal_handler_is_connected (client->proxy,
                                       client->key_pressed_handler))
        g_signal_handler_disconnect (client->proxy,
                                     client->key_pressed_handler);
    if (g_signal_handler_is_connected (client->proxy,
                                       client->key_released_handler))
        g_signal_handler_disconnect (client->proxy,
                                     client->key_released_handler);
}
