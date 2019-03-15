#ifndef KEYEMITTER_H
#define KEYEMITTER_H

#include <glib.h>
#include <X11/XKBlib.h>

#include "eek/eek.h"

typedef struct {
    gint group;
} EekboardContext;

typedef struct {
    EekboardContext *context;
    XkbDescRec *xkb;
    guint modifier_keycodes[8];
} Client;

void
emit_key_activated (EekboardContext *context,
                  guint            keycode,
                  EekSymbol       *symbol,
                  guint            modifiers,
                  Client *client);

gboolean
client_enable_xtest (Client *client);

void
client_disable_xtest (Client *client);
#endif // KEYEMITTER_H
