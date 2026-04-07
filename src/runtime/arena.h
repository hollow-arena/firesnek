#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>
#include <stdint.h>
#include "duckbox.h"

#define ESTUS__ARENA_DEF_SIZE 10           // 8KB arena, sized in ducks. Stored as power of 2 value, so unpack with 1ULL << cap
#define ESTUS__ARENA_MAX_SIZE 32           // 32GB max size per 64-bit constraints
#define ESTUS__DEF_ARENA_NUM  (1 << 6)     // 64 arenas to start
#define ESTUS__MAX_ARENA_NUM  (1UL << 16)  // 2^16 max arenas

// cap is weird so struct fits in 16 bytes
typedef struct estus__arena_s {
    estus__duck *memory;
    uint32_t     bumper;
    uint16_t     id;
    uint16_t     log_cap; // (1ULL << cap) to get actual capacity in ducks
} estus__arena;

typedef struct estus__registry_s {
    estus__arena *arenas;    // Pointer to array of arena structs (metadata)
    uint64_t     *available; // bitmap of available arenas, 0 is available, 1 is occupied
                             // up to 2^16 / 64 = 1024 uint64_t words at max arenas
    uint16_t      cap;       // arena capacity (number of arena slots)
    uint16_t      idx;       // cursor into bitmap word array; stays on words with free slots
} estus__registry;

// ─── Arena-aware duck unpack ─────────────────────────────────────────────────
// Defined here (not duckbox.h) to avoid a circular include — duckbox.h has no
// knowledge of arena layout; arena.h owns these since they touch both types.

static inline estus__duck* estus__unpackp(estus__registry *registry, estus__duck duck) {
    uint16_t arena_id = (duck & 0x0000FFFF00000000ULL) >> 32;
    uint32_t offset   =  duck & 0x00000000FFFFFFFFULL;
    return &registry->arenas[arena_id].memory[offset];
}

static inline estus__arena* estus__get_arena_loc(estus__registry *registry, estus__duck duck) {
    uint16_t arena_id = (duck & 0x0000FFFF00000000ULL) >> 32;
    return &registry->arenas[arena_id];
}

estus__registry estus__registry_create(void);
void            estus__registry_resize(estus__registry *registry);
void            estus__registry_free(estus__registry registry);

uint16_t        estus__arena_create(estus__registry *registry);
estus__duck     estus__arena_alloc(estus__arena *arena, size_t obj_size, estus__type_enum ref_type);
estus__duck     estus__arena_alloc_copy(estus__arena *arena, void *obj, size_t obj_size, estus__type_enum ref_type);
void            estus__arena_free(estus__registry *registry, uint16_t arena_id);
void            estus__arena_clear(estus__registry *registry, uint16_t arena_id);

#endif // ARENA_H
