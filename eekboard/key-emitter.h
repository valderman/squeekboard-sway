#ifndef KEYEMITTER_H
#define KEYEMITTER_H

#include <inttypes.h>
#include <glib.h>
#include <X11/XKBlib.h>

#include "eek/eek.h"

#include "virtual-keyboard-unstable-v1-client-protocol.h"

void
emit_key_activated (EekboardContextService *manager, EekKeyboard *keyboard,
                    guint            keycode,
                    EekSymbol       *symbol,
                    guint            modifiers,
                    gboolean pressed, uint32_t timestamp);
#endif // KEYEMITTER_H
