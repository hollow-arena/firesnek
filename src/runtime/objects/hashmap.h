#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "../arena.h"

// TODO: hashmap uses raw void* key/value — needs porting to estus duck model.
// Arena is aliased here as a placeholder until that port happens.
typedef estus__arena Arena;

// ── Node ──────────────────────────────────────────────────────────────────────
// Stores a heterogeneous key-value pair with size metadata.
// First node in each bucket lives inline in the contiguous bucket array.
// Collision nodes are arena-allocated and linked via bucket_next.
// Insertion order is maintained via order_next.

typedef struct Node {
    void        *key;
    void        *value;
    size_t       keySize;
    size_t       valueSize;
    struct Node *bucket_next;   // next node in same bucket (collision chain)
    struct Node *order_next;    // next node in insertion order
} Node;

// ── HashMap ───────────────────────────────────────────────────────────────────
// Ordered dictionary. Insertion order is preserved via first/last pointers.
// Load factor threshold: 75%. Doubles capacity on resize.

typedef struct {
    Node    *buckets;   // contiguous bucket array (inline first nodes)
    Node    *first;     // head of insertion-order chain
    Node    *last;      // tail of insertion-order chain
    unsigned capacity;
    unsigned len;
} HashMap;

// ── Core operations ───────────────────────────────────────────────────────────

// Allocate and initialise a new HashMap with default capacity.
HashMap    *CreateDict(Arena *arena);

// Insert or update key/value. Resizes at 75% load. Returns false on OOM.
bool        AddDict(Arena *arena, HashMap *dict, Node *dict_buckets,
                    void *key, void *value, size_t keySize, size_t valueSize);

// Double the bucket array and rehash all entries. Returns false on OOM.
bool        ResizeDict(Arena *arena, HashMap *dict);

// Reset the map to empty without freeing arena memory.
void        ClearDict(HashMap *dict);

// ── Hashing ───────────────────────────────────────────────────────────────────

// Polynomial rolling hash finalised with a Knuth multiplicative step.
unsigned    GetHashCode(void *keyPtr, size_t keySize);

// ── TODO ──────────────────────────────────────────────────────────────────────
// bool     ContainsDict(HashMap *dict, void *key, size_t keySize);
// bool     TryGetValue(HashMap *dict, void *key, size_t keySize, void **outValue, size_t *outSize);
// bool     RemoveDict(Arena *arena, HashMap *dict, void *key, size_t keySize);
// Node    *KeysDict(HashMap *dict);    // iterable of keys in insertion order
// Node    *ValuesDict(HashMap *dict);  // iterable of values in insertion order
// Bitwise set operations (union, intersection, difference) for HashSet use
