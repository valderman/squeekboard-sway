#ifndef WAYLAND_H
#define WAYLAND_H

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <gmodule.h>


struct squeek_wayland {
    struct zwlr_layer_shell_v1 *layer_shell;
    GPtrArray *outputs; // *wl_output
    struct wl_seat *seat;
};


extern struct squeek_wayland *squeek_wayland;


static inline void squeek_wayland_init(struct squeek_wayland *wayland) {
    wayland->outputs = g_ptr_array_new();
}

static inline void squeek_wayland_set_global(struct squeek_wayland *wayland) {
    squeek_wayland = wayland;
}

static inline void squeek_wayland_deinit(struct squeek_wayland *wayland) {
    g_ptr_array_free(wayland->outputs, TRUE);
}

#endif // WAYLAND_H
