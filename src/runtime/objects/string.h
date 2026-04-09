#ifndef STRING_H
#define STRING_H

#include "../duckbox.h"
#include "../arena.h"

estus__duck estus__str_new(estus__registry *registry, estus__arena *arena, const char *init, size_t len);
void        estus__str_init_char_table(estus__registry *registry);
estus__duck estus__str_index_value(estus__registry *registry, estus__duck str, estus__duck duck_idx);
estus__duck estus__str_len(estus__registry* registry, estus__duck str_ptr);
estus__duck estus__str_concat(estus__registry *registry, estus__arena *arena, estus__duck str1, estus__duck str2);
estus__duck estus__str_mult(estus__registry *registry, estus__arena *arena, estus__duck str, estus__duck times);

#endif // STRING_H