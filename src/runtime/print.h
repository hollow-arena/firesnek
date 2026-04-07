#include <stdio.h>
#include "arena.h"

// estus__unpack_str returns a STR duck whose layout is [uint64_t len][char data[]].
// Skip past the len field to get the null-terminated C string for printf.
// No free needed — the buffer lives in the arena and is reclaimed with the frame.
#define ESTUS__STR_CSTR(registry, str_duck) \
    ((char*)estus__unpackp((registry), (str_duck)) + sizeof(uint64_t))

#define estus__print(registry, arena, duck) \
    printf("%s", ESTUS__STR_CSTR((registry), estus__unpack_str((registry), (arena), (duck))))

#define estus__println(registry, arena, duck) \
    printf("%s\n", ESTUS__STR_CSTR((registry), estus__unpack_str((registry), (arena), (duck))))
