#ifndef __LAYOUT_H
#define __LAYOUT_H

#include "inttypes.h"

#include "eek/eek-container.h"
#include "src/keyboard.h"

struct squeek_button;
struct squeek_row;

struct squeek_row *squeek_row_new(int32_t angle);
struct squeek_button *squeek_row_create_button (struct squeek_row *row,
                                          guint keycode, guint oref);
struct squeek_button *squeek_row_create_button_with_state(struct squeek_row *row,
                                    struct squeek_button *source);
void squeek_row_set_angle(struct squeek_row *row, int32_t angle);
int32_t squeek_row_get_angle(struct squeek_row*);

uint32_t squeek_row_contains(struct squeek_row*, struct squeek_button *button);

struct squeek_button* squeek_row_find_key(struct squeek_row*, struct squeek_key *state);

typedef void (*ButtonCallback) (struct squeek_button *button, gpointer user_data);
void squeek_row_foreach(struct squeek_row*,
                            ButtonCallback   callback,
                            gpointer      user_data);

void squeek_row_free(struct squeek_row*);

/*
struct squeek_button *squeek_buttons_find_by_position(
    const struct squeek_buttons *buttons,
    double x, double y,
    double origin_x, double origin_y,
    double angle);
void squeek_buttons_add(struct squeek_buttons*, const struct squeek_button* button);
void squeek_buttons_remove_key(struct squeek_buttons*, const struct squeek_key* key);
*/

struct squeek_button *squeek_button_new(uint32_t keycode, uint32_t oref);
struct squeek_button *squeek_button_new_with_state(const struct squeek_button* source);
uint32_t squeek_button_get_oref(const struct squeek_button*);
EekBounds squeek_button_get_bounds(const struct squeek_button*);
void squeek_button_set_bounds(struct squeek_button* button, EekBounds bounds);

struct squeek_symbol *squeek_button_get_symbol (
    const struct squeek_button *button);
struct squeek_key *squeek_button_get_key(struct squeek_button*);
uint32_t *squeek_button_has_key(const struct squeek_button* button,
                                const struct squeek_key *key);
void squeek_button_print(const struct squeek_button* button);


EekBounds squeek_row_place_keys(struct squeek_row *row, LevelKeyboard *keyboard);
#endif
