#ifndef __IMSERVICE_H
#define __IMSERVICE_H

#include "input-method-unstable-v2-client-protocol.h"
#include "eek/eek-types.h"

struct imservice
{
    struct zwp_input_method_v2 *im;
    EekboardContextService *ui_manager;
};

struct imservice* get_imservice(EekboardContextService *context,
                                struct zwp_input_method_manager_v2 *manager,
                                struct wl_seat *seat);

// Defined in Rust
struct imservice* imservice_new(struct zwp_input_method_v2 *im,
                                EekboardContextService *context);
void imservice_handle_input_method_activate(void *data, struct zwp_input_method_v2 *input_method);
void imservice_handle_input_method_deactivate(void *data, struct zwp_input_method_v2 *input_method);
void imservice_handle_surrounding_text(void *data, struct zwp_input_method_v2 *input_method,
                                       const char *text, uint32_t cursor, uint32_t anchor);
void imservice_handle_commit_state(void *data, struct zwp_input_method_v2 *input_method);

#endif
