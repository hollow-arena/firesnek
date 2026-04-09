#ifndef REF_TYPES_H
#define REF_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include "../duckbox.h"

// This will be the default case for every generic container, consider making customizable in config later
#define DEF_SIZE  32
// Expand containers by double when resizing needed
#define EXP_RATIO 2

// TODO: Lists, linked lists, sets, dictionaries

// TODO: Determine role of tuple in Estus. Could be our proxy for dataclass / struct stored on stack / ValueTuple
// If so, tuple implementation might go in a different module

// This struct will fit in a 16-byte allocation. Max size of a list in arena is 2^31 ducks with current 32GB limit.
// Minus one to account for the metadata struct
#define MAX_CAP   2147483648ULL
// A list of 2^64 ducks would be 128 EXABYTES (impossibly large).
// Still, consider expanding this value and not hardcoding for the 32-bit and 128-bit extensions in later versions.

// TODO: Generated code should include any user-defined objects or imported ref-types

// Strings
typedef struct {
    uint64_t len;            // Max arena size 2^32 * 8 chars per duck = 2^35
    // Don't need capacity or any other metadata. Following data will always just be unsigned chars.
} estus__str_metadata;

// Lists/tuples
typedef struct {
    uint32_t len;
    uint32_t cap;
} estus__list_metadata;

// Metadata struct for remaining reference types
typedef struct {
    // Pointers will never point to a double/float, always the metadata struct
    // Class objects defined by user will also have a uint64_t for container type at header position
    uint64_t container_type; // estus__ref_type enum match
    uint32_t len;
    uint32_t cap;   // Not needed for plain linked list, but won't save on space due to padding
    uint32_t first; // single-arena pointer to first element, if linked-list implementation
                    // 0 if len is 0
    uint32_t last;  // single-arena pointer to last element, if linked-list implementation, for appending new values
                    // 0 if len is 0
} estus__ref_metadata;

// Offset storage can be uint32 since max arena size is 2^32, wraps around if needed

// No custom struct needed for strings / tuples / lists, those are just consecutive chars/ducks.

// for linked lists and sets
typedef struct {
    estus__duck value;
    uint32_t    next_offset; // Positional reference relative to entire arena, where start of arena is 0
                             // 0 if node is last in linked list
    uint32_t    prev_offset; // Positional reference relative to entire arena, where start of arena is 0
                             // 0 if node is first in linked list
} estus__node;

// for dict that implements linked list model for ordering
// dict will use open addressing for placing nodes, with secondary hash function for determining offset from initial bucket
typedef struct {
    estus__duck key;
    estus__duck value;
    uint32_t    next_offset; // Positional reference relative to entire arena, where start of arena is 0
                             // 0 if node is last in linked list
    uint32_t    prev_offset; // Positional reference relative to entire arena, where start of arena is 0
                             // 0 if node is first in linked list
} estus__dict_node;

#endif // REF_TYPES_H
