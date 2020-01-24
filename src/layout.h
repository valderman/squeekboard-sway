#ifndef __LAYOUT_H
#define __LAYOUT_H

#include <inttypes.h>
#include <glib.h>
#include "eek/eek-element.h"
#include "eek/eek-gtk-keyboard.h"
#include "eek/eek-renderer.h"
#include "eek/eek-types.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

enum squeek_arrangement_kind {
    ARRANGEMENT_KIND_BASE = 0,
    ARRANGEMENT_KIND_WIDE = 1,
};

struct squeek_layout;

EekBounds squeek_button_get_bounds(const struct squeek_button*);
const char *squeek_button_get_label(const struct squeek_button*);
const char *squeek_button_get_icon_name(const struct squeek_button*);
const char *squeek_button_get_name(const struct squeek_button*);
const char *squeek_button_get_outline_name(const struct squeek_button*);

void squeek_button_print(const struct squeek_button* button);

struct transformation squeek_layout_calculate_transformation(
        const struct squeek_layout *layout,
        double allocation_width, double allocation_size);

struct squeek_layout *squeek_load_layout(const char *name, uint32_t type);
const char *squeek_layout_get_keymap(const struct squeek_layout*);
enum squeek_arrangement_kind squeek_layout_get_kind(const struct squeek_layout *);
void squeek_layout_free(struct squeek_layout*);

void squeek_layout_release(struct squeek_layout *layout,
                           struct submission *submission,
                           struct transformation widget_to_layout,
                           uint32_t timestamp,
                           EekboardContextService *manager,
                           EekGtkKeyboard *ui_keyboard);
void squeek_layout_release_all_only(struct squeek_layout *layout,
                                    struct submission *submission,
                                    uint32_t timestamp);
void squeek_layout_depress(struct squeek_layout *layout,
                           struct submission *submission,
                           double x_widget, double y_widget,
                           struct transformation widget_to_layout,
                           uint32_t timestamp, EekGtkKeyboard *ui_keyboard);
void squeek_layout_drag(struct squeek_layout *layout,
                        struct submission *submission,
                        double x_widget, double y_widget,
                        struct transformation widget_to_layout,
                        uint32_t timestamp, EekboardContextService *manager,
                        EekGtkKeyboard *ui_keyboard);
void squeek_layout_draw_all_changed(struct squeek_layout *layout, EekRenderer* renderer, cairo_t     *cr);
void squeek_draw_layout_base_view(struct squeek_layout *layout, EekRenderer* renderer, cairo_t     *cr);
#endif
