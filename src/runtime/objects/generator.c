#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "hashmap.h"

bool count(int64_t start, int64_t stop, int64_t step, int64_t *val) {
    if (stop > start && step <= 0) return false;
    if (stop < start && step >= 0) return false;
    if (stop == start) return false;

    *val += step;

    return (step > 0) ? *val < stop : *val > stop;
}

// Called like:
// int64_t i_0 = x - z;   // start - step
// while (count(x, y, z, &i_0)) {
//     // i_0 = x on first iteration, advances by z each time
// }

// Iterating over a hashmap / set / linkedlist is almost easier. Called like:
// Node *ptr = hashmap->first;
// while (ptr) {
//     // Do stuff with *ptr->key and *ptr->value and their sizes as needed
//     ptr = ptr->order_next;
// }
