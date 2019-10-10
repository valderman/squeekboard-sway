#ifndef __OUTPUTS_H
#define __OUTPUTS_H

#include "wayland-client-protocol.h"

struct squeek_outputs;

struct squeek_outputs *squeek_outputs_new();
void squeek_outputs_free(struct squeek_outputs*);
void squeek_outputs_register(struct squeek_outputs*, struct wl_output *output);
struct wl_output *squeek_outputs_get_current(struct squeek_outputs*);

#endif
