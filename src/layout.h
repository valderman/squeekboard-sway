#ifndef __LAYOUT_H
#define __LAYOUT_H

#include <inttypes.h>
#include <glib.h>
#include "eek/eek-element.h"
#include "src/keyboard.h"

struct squeek_button;
struct squeek_row;
struct squeek_view;
struct squeek_layout;

int32_t squeek_row_get_angle(const struct squeek_row*);

EekBounds squeek_row_get_bounds(const struct squeek_row*);

uint32_t squeek_row_contains(struct squeek_row*, struct squeek_button *button);

struct button_place squeek_view_find_key(struct squeek_view*, struct squeek_key *state);


typedef void (*ButtonCallback) (struct squeek_button *button, gpointer user_data);
void squeek_row_foreach(struct squeek_row*,
                            ButtonCallback   callback,
                            gpointer      user_data);

void squeek_row_free(struct squeek_row*);

uint32_t squeek_button_get_oref(const struct squeek_button*);
EekBounds squeek_button_get_bounds(const struct squeek_button*);
const char *squeek_button_get_label(const struct squeek_button*);
const char *squeek_button_get_icon_name(const struct squeek_button*);
const char *squeek_button_get_name(const struct squeek_button*);

struct squeek_key *squeek_button_get_key(const struct squeek_button*);
uint32_t *squeek_button_has_key(const struct squeek_button* button,
                                const struct squeek_key *key);
void squeek_button_print(const struct squeek_button* button);


EekBounds squeek_view_get_bounds(const struct squeek_view*);

typedef void (*RowCallback) (struct squeek_row *row, gpointer user_data);
void squeek_view_foreach(struct squeek_view*,
                            RowCallback   callback,
                            gpointer      user_data);

struct squeek_row *squeek_view_get_row(struct squeek_view *view,
                                       struct squeek_button *button);

struct squeek_button *squeek_view_find_button_by_position(struct squeek_view *view, EekPoint point);


void
squeek_layout_place_contents(struct squeek_layout*);
struct squeek_view *squeek_layout_get_current_view(struct squeek_layout*);
void squeek_layout_set_state_from_press(struct squeek_layout* layout, LevelKeyboard *keyboard, struct squeek_button* button);


struct squeek_layout *squeek_load_layout(const char *type);
const char *squeek_layout_get_keymap(const struct squeek_layout*);
void squeek_layout_free(struct squeek_layout*);
#endif
