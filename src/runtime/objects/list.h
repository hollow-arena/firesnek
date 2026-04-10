#ifndef LIST_H
#define LIST_H

#include "../duckbox.h"
#include "../arena.h"
#include "ref_types.h"

estus__duck estus__list_create(estus__registry *registry, estus__arena *arena, estus__duck *init, uint32_t len);
estus__duck estus__list_len(estus__registry* registry, estus__duck list_ptr);
estus__duck  estus__list_index_value(estus__registry *registry, estus__duck list, estus__duck duck_idx);
estus__duck* estus__list_index_ptr  (estus__registry *registry, estus__duck list, estus__duck duck_idx);
estus__duck estus__list_resize(estus__registry *registry, estus__arena *arena, estus__list_metadata *list);
estus__duck estus__list_concat(estus__registry *registry, estus__arena *arena, estus__duck list1, estus__duck list2);
estus__duck estus__list_mult(estus__registry *registry, estus__arena *arena, estus__duck list, estus__duck times);
void        estus__list_append(estus__registry *registry, estus__duck *list, estus__duck value);
estus__duck estus__list_pop(estus__registry *registry, estus__duck *list);
void        estus__list_insert(estus__registry *registry, estus__duck *list, int64_t idx, estus__duck value);
void        estus__list_remove(estus__registry *registry, estus__duck *list, estus__duck value);
void        estus__list_clear(estus__registry *registry, estus__duck *list);
estus__duck estus__list_find(estus__registry *registry, estus__duck *list, estus__duck value, int64_t start, int64_t end);
void        estus__list_reverse(estus__registry *registry, estus__duck *list);
estus__duck estus__list_str(estus__registry *registry, estus__arena *arena, estus__duck list);

#endif // LIST_H
