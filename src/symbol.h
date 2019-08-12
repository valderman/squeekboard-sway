#ifndef __SYMBOL_H
#define __SYMBOL_H

#include "stdbool.h"
#include "inttypes.h"
// Defined in Rust

struct squeek_symbol;
struct squeek_symbols;

void squeek_symbols_add(struct squeek_symbols*,
                        const char *element_name,
                        const char *text, uint32_t keyval,
                        const char *label, const char *icon,
                        const char *tooltip);


const char *squeek_symbol_get_name(struct squeek_symbol* symbol);
const char *squeek_symbol_get_label(struct squeek_symbol* symbol);
const char *squeek_symbol_get_icon_name(struct squeek_symbol* symbol);
uint32_t squeek_symbol_get_modifier_mask(struct squeek_symbol* symbol);

void squeek_symbol_print(struct squeek_symbol* symbol);
#endif
