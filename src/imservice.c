#include "imservice.h"

#include <glib.h>


void imservice_handle_text_change_cause(void *data, struct zwp_input_method_v2 *input_method) {}
void imservice_handle_content_type(void *data, struct zwp_input_method_v2 *input_method) {}
void imservice_handle_unavailable(void *data, struct zwp_input_method_v2 *input_method) {}


static const struct zwp_input_method_v2_listener input_method_listener = {
    .activate = imservice_handle_input_method_activate,
    .deactivate = imservice_handle_input_method_deactivate,
    .surrounding_text = imservice_handle_surrounding_text,
    .text_change_cause = imservice_handle_text_change_cause,
    .content_type = imservice_handle_content_type,
    .done = imservice_handle_commit_state,
    .unavailable = imservice_handle_unavailable,
};

struct imservice* get_imservice(struct zwp_input_method_manager_v2 *manager,
                                struct wl_seat *seat) {
    struct zwp_input_method_v2 *im = zwp_input_method_manager_v2_get_input_method(manager, seat);
    struct imservice *imservice = imservice_new(im);
    zwp_input_method_v2_add_listener(im,
        &input_method_listener, imservice);
    return imservice;
}

void imservice_make_visible(struct imservice *imservice) {
    g_log("squeek", G_LOG_LEVEL_DEBUG, "Visibiling");
}
void imservice_try_hide(struct imservice *imservice) {
    g_log("squeek", G_LOG_LEVEL_DEBUG, "Hiding");
}
