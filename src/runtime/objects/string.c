#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../duckbox.h"
#include "../arena.h"
#include "../error.h"
#include "ref_types.h"
#include "string.h"

estus__duck estus__str_new(estus__registry *registry, estus__arena *arena, const char* init, size_t len) {

    // No frees required on failure here, arena auto frees on error/exit command

    // Max cap only applies to 64-bit ducks, chars are 8 bits
    if (len > MAX_CAP * 8) estus__panic_roll(ESTUS_ERR_MEMORY, "strings longer than 2^34 chars not supported on 64-bit");

    uint64_t first_sizing = DEF_SIZE;
    while (first_sizing < len) first_sizing <<= 1;

    estus__duck str_alloc = estus__arena_alloc(arena, sizeof(estus__str_metadata) + sizeof(char) * (first_sizing + 1), STR); // +1 for null terminator
    // arena panics if allocation fails
    
    estus__str_metadata str_data = { .len = len };
    memcpy(estus__unpackp(registry, str_alloc), &str_data, sizeof(estus__str_metadata));
    if (len > 0 && init) memcpy((char*)estus__unpackp(registry, str_alloc) + sizeof(estus__str_metadata), init, len + 1);

    return str_alloc;
}

static inline char* estus__str_data(estus__str_metadata *meta) {
    return (char*)meta + sizeof(estus__str_metadata);
}

estus__duck estus__str_len(estus__registry* registry, estus__duck str_ptr) {
    return estus__packi(((estus__str_metadata*)estus__unpackp(registry, str_ptr))->len);
}

void estus__str_init_char_table(estus__registry *registry) {
    uint16_t perm_id = estus__arena_create(registry);  // gets arena 0, bitmap bit set — never reused
    estus__arena *perm = &registry->arenas[perm_id];
    for (int i = 0; i < 256; i++) {
        char ch = (char)i;
        registry->char_table[i] = estus__str_new(registry, perm, &ch, 1);
    }
}

estus__duck estus__str_index_value(estus__registry *registry, estus__duck str, estus__duck duck_idx) {
    estus__str_metadata *meta = (estus__str_metadata*)estus__unpackp(registry, str);
    int64_t idx = estus__unpacki(duck_idx);

    if (idx < -(int64_t)meta->len || idx >= (int64_t)meta->len)
        estus__panic_roll(ESTUS_ERR_INDEX, "string index out of range");

    if (idx < 0) idx += meta->len;

    return registry->char_table[(unsigned char)estus__str_data(meta)[idx]];
}

estus__duck estus__str_concat(estus__registry *registry, estus__arena *arena, estus__duck str1, estus__duck str2) {
    estus__str_metadata* meta1 = (estus__str_metadata*)estus__unpackp(registry, str1);
    estus__str_metadata* meta2 = (estus__str_metadata*)estus__unpackp(registry, str2);

    estus__duck new_str = estus__str_new(registry, arena, NULL, meta1->len + meta2->len); // Null terminator accounted for in constructor
    char* ptr = estus__str_data((estus__str_metadata*)estus__unpackp(registry, new_str));

    memcpy(ptr, estus__str_data(meta1), meta1->len);
    memcpy(ptr + meta1->len, estus__str_data(meta2), meta2->len + 1); // Include null-terminator from 2nd string
    return new_str;
}

estus__duck estus__str_mult(estus__registry *registry, estus__arena *arena, estus__duck str, estus__duck times) {
    estus__str_metadata* meta = (estus__str_metadata*)estus__unpackp(registry, str);
    uint32_t times_int = estus__unpacki(times);
    estus__duck new_str = estus__str_new(registry, arena, NULL, meta->len * times_int);
    char* new_str_data = estus__str_data((estus__str_metadata*)estus__unpackp(registry, new_str));
    char* old_str_data = estus__str_data((estus__str_metadata*)estus__unpackp(registry, str));

    // Loop as many times as needed
    for (unsigned i = 0; i < times_int; i++) {
        memcpy(&new_str_data[i * meta->len], old_str_data, meta->len);
    }
    // Add null terminator
    new_str_data[meta->len * times_int] = (char)0;
    return new_str;
}