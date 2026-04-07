#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include "../arena.h"
#include "hashmap.h"

// TODO: replace with estus arena allocation once hashmap is ported to duck model.
static void* AllocArena(Arena *arena, size_t size) { (void)arena; return malloc(size); }
static void* AllocArenaCopy(Arena *arena, void *src, size_t size) {
    (void)arena; void *p = malloc(size); if (p) memcpy(p, src, size); return p;
}

#define DEFAULT_SIZE 32
#define MAX_SIZE UINT_MAX
#define SMALL_PRIME 31
#define LARGE_PRIME 2654435761u

// TODO: Consider adding optional parameter for initial sizing at some point?

// Returns a Node by value for inline placement in the bucket array.
// key == NULL indicates allocation failure.
Node CreateFirstNode(Arena *arena, void *key, void *value, size_t keySize, size_t valueSize) {
    void *k = AllocArenaCopy(arena, key, keySize);
    if (!k) return (Node){0};
    void *v = AllocArenaCopy(arena, value, valueSize);
    if (!v) return (Node){0};

    return (Node){
        .key       = k,   .value     = v,
        .keySize   = keySize, .valueSize = valueSize,
        .bucket_next = NULL,  .order_next = NULL,
    };
}

Node *CreateNode(Arena *arena, void *key, void *value, size_t keySize, size_t valueSize) {
    Node *node = AllocArena(arena, sizeof(Node));
    if (!node) return NULL;

    void *k = AllocArenaCopy(arena, key, keySize);
    if (!k) return NULL;
    void *v = AllocArenaCopy(arena, value, valueSize);
    if (!v) return NULL;

    node->key         = k;
    node->value       = v;
    node->keySize     = keySize;
    node->valueSize   = valueSize;
    node->bucket_next = NULL;
    node->order_next  = NULL;

    return node;
}

HashMap *CreateDict(Arena *arena) {
    HashMap *dict = AllocArena(arena, sizeof(HashMap));
    if (!dict) return NULL;

    dict->buckets = AllocArena(arena, DEFAULT_SIZE * sizeof(Node));
    if (!dict->buckets) return NULL;
    memset(dict->buckets, 0, DEFAULT_SIZE * sizeof(Node));
    dict->first    = NULL;
    dict->last     = NULL;
    dict->capacity = DEFAULT_SIZE;
    dict->len      = 0;

    return dict;
}

unsigned GetHashCode(void *keyPtr, size_t keySize) {
    unsigned char *ptr = (unsigned char *)keyPtr;
    unsigned int hash  = 0;
    for (size_t i = 0; i < keySize; i++) {
        hash = hash * SMALL_PRIME + ptr[i];
    }
    return (hash * LARGE_PRIME);
}

bool ResizeDict(Arena *arena, HashMap *dict);

bool AddDict(Arena *arena, HashMap *dict, Node *dict_buckets, void *key, void *value, size_t keySize, size_t valueSize) {
    unsigned bucket = GetHashCode(key, keySize) % dict->capacity;
    Node    *ptr    = &dict_buckets[bucket];

    // Empty bucket slot — place node inline in the contiguous array
    if (ptr->key == NULL) {
        dict_buckets[bucket] = CreateFirstNode(arena, key, value, keySize, valueSize);
        if (dict_buckets[bucket].key == NULL) return false;

        Node *slot = &dict_buckets[bucket];
        if (!dict->first) {
            dict->first = slot;
        } else {
            dict->last->order_next = slot;
        }
        dict->last = slot;
        dict->len++;
        return true;
    }

    // Walk the chain — update existing key or find end
    Node *prev = NULL;
    while (ptr) {
        if (ptr->keySize == keySize && memcmp(ptr->key, key, keySize) == 0) {
            void *v = AllocArenaCopy(arena, value, valueSize);
            if (!v) return false;
            ptr->value     = v;
            ptr->valueSize = valueSize;
            return true;
        }
        prev = ptr;
        ptr  = ptr->bucket_next;
    }

    // New collision node — heap allocated, copies key and value
    if (dict->len >= dict->capacity * 3 / 4)
        if (!ResizeDict(arena, dict)) return false;

    Node *node = CreateNode(arena, key, value, keySize, valueSize);
    if (!node) return false;

    prev->bucket_next      = node;
    dict->last->order_next = node;
    dict->last             = node;
    dict->len++;

    return true;
}

bool ResizeDict(Arena *arena, HashMap *dict) {
    unsigned newCapacity = dict->capacity * 2;
    Node    *newBuckets  = AllocArena(arena, sizeof(Node) * newCapacity);
    if (!newBuckets) return false;
    memset(newBuckets, 0, sizeof(Node) * newCapacity);

    Node *newFirst = NULL;
    Node *newLast  = NULL;

    Node *cur = dict->first;
    while (cur) {
        Node *next       = cur->order_next;  // save before clobbering
        cur->bucket_next = NULL;
        cur->order_next  = NULL;

        unsigned newBucket = GetHashCode(cur->key, cur->keySize) % newCapacity;

        Node *slot;
        if (newBuckets[newBucket].key == NULL) {
            // Inline copy into the new contiguous slot
            newBuckets[newBucket] = *cur;
            slot = &newBuckets[newBucket];
        } else {
            // Collision — walk to tail and link cur directly (valid arena memory)
            Node *tail = &newBuckets[newBucket];
            while (tail->bucket_next) tail = tail->bucket_next;
            tail->bucket_next = cur;
            slot = cur;
        }

        // Rebuild insertion-order chain
        if (!newFirst) newFirst = slot;
        else           newLast->order_next = slot;
        newLast = slot;

        cur = next;
    }

    dict->buckets  = newBuckets;
    dict->capacity = newCapacity;
    dict->first    = newFirst;
    dict->last     = newLast;

    return true;
}

// Note, we don't need to free anything since we're using arena
void ClearDict(HashMap *dict) {
    memset(dict->buckets, 0, dict->capacity * sizeof(Node));
    dict->first = NULL;
    dict->last  = NULL;
    dict->len   = 0;
}

// TODO: Add rest of core methods: Contains, TryGetValue, Remove, iterable of keys and/or values
// Also bitwise ops for hashsets
