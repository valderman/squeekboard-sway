#ifndef __SYMBOL_H
#define __SYMBOL_H

#include "stdbool.h"
#include "inttypes.h"
// Defined in Rust

struct squeek_symbol;
struct squeek_symbols;

struct squeek_symbol* squeek_symbol_new(const char *element_name,
                                        const char *text, uint32_t keyval,
                                        const char *label, const char *icon,
                                        const char *tooltip);


const char *squeek_symbol_get_name(struct squeek_symbol* symbol);
const char *squeek_symbol_get_label(struct squeek_symbol* symbol);
const char *squeek_symbol_get_icon_name(struct squeek_symbol* symbol);
uint32_t squeek_symbol_get_modifier_mask(struct squeek_symbol* symbol);

void squeek_symbol_print(struct squeek_symbol* symbol);

struct squeek_symbols* squeek_symbols_new();
void squeek_symbols_free(struct squeek_symbols*);
void squeek_symbols_append(struct squeek_symbols*, struct squeek_symbol *item);
struct squeek_symbol *squeek_symbols_get(struct squeek_symbols*, uint32_t level);

const char* squeek_key_to_keymap_entry(const char *key_name, struct squeek_symbols *symbols);
#endif
