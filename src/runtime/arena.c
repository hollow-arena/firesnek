#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "arena.h"
#include "duckbox.h"
#include "error.h"

// Registry stored on entry point's stack frame
estus__registry estus__registry_create(void) {
    estus__registry registry;

    registry.arenas    = calloc(ESTUS__DEF_ARENA_NUM, sizeof(estus__arena));
    registry.available = calloc(ESTUS__DEF_ARENA_NUM / 64, sizeof(uint64_t));

    if (!registry.arenas || !registry.available)
        estus__panic_roll(ESTUS_ERR_MEMORY, "Failed to register initial arena allocators");

    registry.cap = ESTUS__DEF_ARENA_NUM;
    registry.idx = 0;

    return registry;
}

// THIS IS NOT PROPERLY IMPLEMENTED YET — needs to resize available bitmap too, do NOT use
void estus__registry_resize(estus__registry *registry) {
    estus__arena *new_arenas = realloc(registry->arenas, registry->cap * 2 * sizeof(estus__arena));

    if (!new_arenas)
        estus__panic_roll(ESTUS_ERR_MEMORY, "Failed to reallocate new arena pointers");

    registry->arenas = new_arenas;
    registry->cap   *= 2;
}

void estus__registry_free(estus__registry registry) {
    free(registry.arenas);
    free(registry.available);
}

// Scans bitmap words from cursor position, wraps around; updates idx to the word with a free slot
static inline uint8_t estus__get_free_index(estus__registry *registry) {
    uint16_t num_words = registry->cap / 64;

    for (uint16_t i = 0; i < num_words; i++) {
        uint16_t word_idx = (registry->idx + i) % num_words;
        uint64_t curr_set = registry->available[word_idx];
        if (~curr_set) {
            registry->idx = word_idx;
            return __builtin_ffsll(~curr_set) - 1;
        }
    }

    // TODO: implement registry resize for call depth > ESTUS__DEF_ARENA_NUM
    estus__panic_roll(ESTUS_ERR_NOTIMPLEMENTED, "Arena count resizing not yet implemented");
    return 0;
}

// Arena struct lives on heap; returns arena_id for registry lookup
uint16_t estus__arena_create(estus__registry *registry) {
    estus__arena arena;

    arena.memory = malloc(sizeof(estus__duck) * (1UL << ESTUS__ARENA_DEF_SIZE));

    if (!arena.memory)
        estus__panic_roll(ESTUS_ERR_MEMORY, "Could not allocate arena memory for this function call");

    arena.bumper = 0;
    arena.log_cap = ESTUS__ARENA_DEF_SIZE;

    uint8_t  bit_idx  = estus__get_free_index(registry);
    uint16_t arena_id = registry->idx * 64 + bit_idx;

    arena.id = arena_id;

    registry->arenas[arena_id]          = arena;
    registry->available[registry->idx] |= 1ULL << bit_idx;

    return arena_id;
}

static void estus__memory_resize(estus__arena *arena, uint8_t new_cap) {
    estus__duck *new_memory = realloc(arena->memory, (1ULL << new_cap) * sizeof(estus__duck));
    if (!new_memory)
        estus__panic_roll(ESTUS_ERR_MEMORY, "Could not resize arena for this stack frame");

    arena->memory = new_memory;
    arena->log_cap = new_cap;
}

// Returns packed PTR duck encoding (arena_id, offset) into arena memory
estus__duck estus__arena_alloc(estus__arena *arena, size_t obj_size, estus__type_enum ref_type) {
    uint64_t cap = (1ULL << arena->log_cap);
    uint8_t log_cap = arena->log_cap;
    while (cap - arena->bumper < obj_size && log_cap < ESTUS__ARENA_MAX_SIZE) {
        cap *= 2;
        log_cap++;
    }

    if (cap - arena->bumper < obj_size)
        estus__panic_roll(ESTUS_ERR_MEMORY, "Heap memory overflow, max arena size reached");

    if (log_cap > arena->log_cap) estus__memory_resize(arena, log_cap);

    uint32_t offset = arena->bumper;
    arena->bumper  += obj_size;
    return estus__packp(arena->id, offset, ref_type);
}

// TODO: Update to work with chars/bytes
estus__duck estus__arena_alloc_copy(estus__arena *arena, void *obj, size_t obj_size, estus__type_enum ref_type) {
    estus__duck slot   = estus__arena_alloc(arena, obj_size, ref_type);
    uint32_t    offset = slot & 0x00000000FFFFFFFFULL;
    memcpy(&arena->memory[offset], obj, obj_size * sizeof(estus__duck));
    return slot;
}

// Frees arena memory and struct, clears bitmap slot for reuse
void estus__arena_free(estus__registry *registry, uint16_t arena_id) {
    estus__arena *arena = &registry->arenas[arena_id];
    if (!arena) return;

    free(arena->memory);
    // Don't need to free arena itself, that frees when register frees

    memset(&registry->arenas[arena_id], 0, sizeof(estus__arena));
    registry->available[arena_id / 64] &= ~(1ULL << (arena_id % 64));
}

// Resets arena bumper and marks slot available without freeing the allocation
void estus__arena_clear(estus__registry *registry, uint16_t arena_id) {
    estus__arena *arena = &registry->arenas[arena_id];
    if (!arena) return;

    arena->bumper = 0;
    // Arena is NOT freed from registry at this point
}
