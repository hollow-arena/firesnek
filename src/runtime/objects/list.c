#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../duckbox.h"
#include "../arena.h"
#include "../error.h"
#include "ref_types.h"

estus__duck estus__list_create(estus__registry *registry, estus__arena *arena, estus__duck *init, uint32_t len) {

    // No frees required on failure here, arena auto frees on error/exit command

    if (len > MAX_CAP) estus__panic_roll(ESTUS_ERR_MEMORY, "lists longer than 2^31 items not supported on 64-bit");

    uint32_t first_sizing = DEF_SIZE;
    while (first_sizing < len && first_sizing < MAX_CAP) first_sizing <<= 1;

    estus__duck list_alloc = estus__arena_alloc(arena, sizeof(estus__list_metadata) + sizeof(estus__duck) * first_sizing, LIST);
    if (!list_alloc) {
        estus__panic_roll(ESTUS_ERR_MEMORY, "failed to allocate list metadata in arena memory");
        return 0;
    }

    estus__list_metadata list_data = { .len = len, .cap = first_sizing };
    memcpy(estus__unpackp(registry, list_alloc), &list_data, sizeof(estus__list_metadata));
    if (len > 0 && init) memcpy((uint8_t*)estus__unpackp(registry, list_alloc) + sizeof(estus__list_metadata), init, len * sizeof(estus__duck));


    return list_alloc;
}

static inline estus__duck* estus__list_data(estus__list_metadata *meta) {
    return (estus__duck*)((char*)meta + sizeof(estus__list_metadata));
}

estus__duck estus__get_index_value(estus__registry *registry, estus__duck list, estus__duck duck_idx) {
    int64_t idx = estus__unpacki(duck_idx);
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, list);

    if (idx < -(int64_t)data->len || idx >= (int64_t)data->len) {
        estus__panic_roll(ESTUS_ERR_INDEX, "Given index out of range");
        return 0;
    }

    if (idx < 0) idx += data->len;

    return estus__list_data(data)[idx];
}

// TODO: Consider after moving resized list to arena bumper: compacting and updating local pointers as needed. Not urgent.
// Returns new location of resized list, make sure to assign when calling
// TODO: Allow check to see if can realloc in place if list is final item in arena
estus__duck estus__list_resize(estus__registry *registry, estus__arena *arena, estus__list_metadata *list) {
    uint32_t new_size = list->cap * 2;
    if (new_size > MAX_CAP) estus__panic_roll(ESTUS_ERR_MEMORY, "lists longer than 2^31 items not supported on 64-bit");
    estus__duck new_list = estus__arena_alloc(arena, sizeof(estus__list_metadata) + new_size * sizeof(estus__duck), LIST);
    if (!new_list)
        // This should ideally not fire, arena allocator should catch error
        estus__panic_roll(ESTUS_ERR_MEMORY, "failed to resize list");

    list->cap *= 2;
    memcpy(estus__unpackp(registry, new_list), list, sizeof(estus__list_metadata) + sizeof(estus__duck) * list->len);
    return new_list;
}

void estus__list_append(estus__registry *registry, estus__duck *list, estus__duck value) {
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, *list);
    if (data->len == data->cap) {
        *list = estus__list_resize(registry, estus__get_arena_loc(registry, *list), data);
        data = (estus__list_metadata*)estus__unpackp(registry, *list);
    }

    estus__list_data(data)[data->len++] = value;
}

estus__duck estus__list_pop(estus__registry *registry, estus__duck *list) {
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, *list);
    if (data->len == 0) {
        estus__panic_roll(ESTUS_ERR_INDEX, "Cannot pop from an empty list");
        return 0;
    }

    return estus__list_data(data)[--data->len];
}

// A type check will have to be inlined into the compiler such that the idx argument is ALWAYS an int duck
void estus__list_insert(estus__registry *registry, estus__duck *list, int64_t idx, estus__duck value) {
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, *list);

    if (idx < -(int64_t)data->len || idx >= (int64_t)data->len) {
        estus__panic_roll(ESTUS_ERR_INDEX, "Given index out of range");
        return;
    }

    if (idx < 0) idx += data->len;

    if (data->len == data->cap) {
        *list = estus__list_resize(registry, estus__get_arena_loc(registry, *list), data);
        data = (estus__list_metadata*)estus__unpackp(registry, *list);
    }

    memmove(estus__list_data(data) + idx + 1, estus__list_data(data) + idx, (data->len++ - idx) * sizeof(estus__duck));
    estus__list_data(data)[idx] = value;
}

void estus__list_remove(estus__registry *registry, estus__duck *list, estus__duck value) {
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, *list);

    for (uint32_t i = 0; i < data->len; i++) {
        if (estus__list_data(data)[i] == value) {
            memmove(estus__list_data(data) + i, estus__list_data(data) + i + 1, (--data->len - i) * sizeof(estus__duck));
            return;
        }
    }

    estus__panic_roll(ESTUS_ERR_VALUE, "Given value not found in list");
}

void estus__list_clear(estus__registry *registry, estus__duck *list) {
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, *list);
    data->len = 0;
}

estus__duck estus__list_find(estus__registry *registry, estus__duck *list, estus__duck value, int64_t start, int64_t end) {
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, *list);

    if (start < -(int64_t)data->len || start >= (int64_t)data->len) {
        estus__panic_roll(ESTUS_ERR_INDEX, "Given index out of range for argument 'start'");
        return 0;
    }

    if (end < -(int64_t)data->len || end >= (int64_t)data->len) {
        estus__panic_roll(ESTUS_ERR_INDEX, "Given index out of range for argument 'end'");
        return 0;
    }

    if (start < 0) start += data->len;
    if (end < 0)   end   += data->len;

    if (start > end) {
        estus__panic_roll(ESTUS_ERR_INDEX, "Start index cannot exceed ending index");
        return 0;
    }

    // end is exclusive, just like Python
    for (uint32_t i = start; i < end; i++) {
        if (estus__list_data(data)[i] == value) return estus__packi(i);
    }

    return estus__packi(-1);
}

void estus__list_reverse(estus__registry *registry, estus__duck *list) {
    estus__list_metadata *data = (estus__list_metadata*)estus__unpackp(registry, *list);
    estus__duck *lo = estus__list_data(data);
    estus__duck *hi = lo + data->len - 1;

    while (lo < hi) {
        estus__duck tmp = *lo;
        *lo++ = *hi;
        *hi-- = tmp;
    }
}

// TODO: Sort scares me
