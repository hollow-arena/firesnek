#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "arena.h"
#include "error.h"
#include "duckbox.h"

// EVERY standard unpack will be either a 1 or a 0 (for truthy vs falsy checks).
// Will also include a string unpack function to make print() calls easy.
// All other binary expressions should be evaluated with internal duck functions.

// Only for direct truthy vs falsy casts, i.e. "if (some_integer)"
bool estus__unpack_truthy(estus__duck duck) {
    switch (_get_type_enum(duck)) {
        case FLOAT:     { double d; memcpy(&d, &duck, 8); return d != 0.0; }
        case INT:
        case UINT:      return (duck & 0x0007FFFFFFFFFFFFULL) != 0;
        case CHAR:      return estus__unpackc(duck) != '\0';
        case BYTE:      return (duck & 0xFF) != 0;
        case BOOL:      return duck & 1;
        case NONE:
        case _NAN:      return 0;
        // TODO: check len field in metadata for empty string/container once PTR semantics are fleshed out
        case STR:
        case STRBYTES:
        case LIST:
        case LISTBYTES:
        case TUPLE:
        case SET:
        case DICT:
        case DEQUE:
        case FUNC:
        case CLOSURE:
        case OBJ:       return 1;
    }
    return 0;
}

// TODO: Make printable strings beyond 64 chars.
// str layout: [uint64_t len][char data[64]] — mirrors estus__str_metadata in ref_types.h
#define ESTUS__UNPACK_STR_CAP 64
estus__duck estus__unpack_str(estus__registry *registry, estus__arena *arena, estus__duck duck) {
    estus__duck str_duck = estus__arena_alloc(arena, sizeof(uint64_t) + ESTUS__UNPACK_STR_CAP, STR);

    uint64_t *len_field = (uint64_t*)estus__unpackp(registry, str_duck);
    char     *buf       = (char*)(len_field + 1);

    switch (_get_type_enum(duck)) {
        case FLOAT: { double d; memcpy(&d, &duck, 8);
                      *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "%g", d);             break; }
        case INT:   *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "%" PRId64,
                                          estus__unpacki(duck));                               break;
        case UINT:  *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "%" PRIu64,
                                          estus__unpacku(duck));                               break;
        case CHAR:  *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "%c",
                                          estus__unpackc(duck));                               break;
        case BYTE:  *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "%02x",
                                          (unsigned char)(duck & 0xFF));                       break;
        case BOOL:  *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "%s",
                                          estus__unpackb(duck) ? "true" : "false");            break;
        case NONE:  *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "none");                break;
        case _NAN:  *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "nan");                 break;
        // TODO: walk arena metadata to print actual string/container contents
        case STR:
        case STRBYTES:  *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<str at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case LIST:
        case LISTBYTES: *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<list at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case TUPLE:     *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<tuple at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case SET:       *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<set at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case DICT:      *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<dict at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case DEQUE:     *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<deque at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case FUNC:      *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<func at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case CLOSURE:   *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<closure at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
        case OBJ:       *len_field = snprintf(buf, ESTUS__UNPACK_STR_CAP, "<obj at %u>",
                                              (uint32_t)(duck & 0xFFFFFFFFULL));               break;
    }
    return str_duck;
}

// Arithmetic ops will be generally allowed between different types.
// Promotion priority: Float > Int
// Char arithmetic is disallowed except (+) which returns a string pointer.
// User should cast char to int with int() or ord().
// Bool - Always 1 or 0. Arithmetic on two bools always results in an int.
// None will always be interpreted as 0.
// Pointer arithmetic is not supported for v1.

// TODO: Arbitrary precision ints — when an INT/UINT arithmetic result exceeds 2^50-1, allocate a bigint
// struct on the arena and return a tagged PTR duck instead.

static estus__duck _char_to_int(estus__duck duck) {
    unsigned char c = estus__unpackc(duck);
    if (c < '0' || c > '9') { estus__panic_roll(ESTUS_ERR_TYPE, "invalid char cast to int"); return 0ULL; }
    return estus__packi(c - '0');
}

static estus__duck _char_to_float(estus__duck duck) {
    unsigned char c = estus__unpackc(duck);
    if (c < '0' || c > '9') { estus__panic_roll(ESTUS_ERR_TYPE, "invalid char cast to float"); return 0ULL; }
    return estus__packf((double)(c - '0'));
}

estus__duck estus__casti(estus__duck duck) {
    switch (_get_type_enum(duck)) {
        case FLOAT: return estus__packi((int64_t)estus__unpackf(duck));
        case INT:   return duck;
        case UINT:  return estus__packi((int64_t)estus__unpacku(duck));
        case CHAR:  return _char_to_int(duck);
        case BYTE:  estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast bytes to int");              break;
        case BOOL:  return estus__packi((int64_t)estus__unpackb(duck));
        case NONE:  return estus__packi(0);
        case _NAN:  estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast nan to int");               break;
        case STR:
        case STRBYTES:
        case LIST: case LISTBYTES: case TUPLE: case SET:
        case DICT: case DEQUE: case FUNC: case CLOSURE: case OBJ:
                    estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast reference type to int");    break;
    }
    return 0ULL;
}

estus__duck estus__castf(estus__duck duck) {
    switch (_get_type_enum(duck)) {
        case FLOAT: return duck;
        case INT:   return estus__packf((double)estus__unpacki(duck));
        case UINT:  return estus__packf((double)estus__unpacku(duck));
        case CHAR:  return _char_to_float(duck);
        case BYTE:  estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast bytes to float");           break;
        case BOOL:  return estus__packf((double)estus__unpackb(duck));
        case NONE:  return estus__packf(0.0);
        case _NAN:  return duck;
        case STR:
        case STRBYTES:
        case LIST: case LISTBYTES: case TUPLE: case SET:
        case DICT: case DEQUE: case FUNC: case CLOSURE: case OBJ:
                    estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast reference type to float");  break;
    }
    return 0ULL;
}

estus__duck estus__castb(estus__duck duck) {
    switch (_get_type_enum(duck)) {
        case FLOAT: return estus__packb(estus__unpackf(duck) != 0.0);
        case INT:   return estus__packb(estus__unpacki(duck) != 0);
        case UINT:  return estus__packb(estus__unpacku(duck) != 0);
        case CHAR:  return estus__packb(estus__unpackc(duck) != '\0');
        case BYTE:  return estus__packb((duck & 0xFF) != 0);
        case BOOL:  return duck;
        case NONE:  return estus__packb(false);
        case _NAN:  return estus__packb(false);
        // TODO: Empty strings/containers are false, non-empty are true
        case STR:
        case STRBYTES:
        case LIST: case LISTBYTES: case TUPLE: case SET:
        case DICT: case DEQUE: case FUNC: case CLOSURE: case OBJ:
                    estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast reference type to bool");   break;
    }
    return 0ULL;
}

// TODO: chr(int) — returns CHAR duck of the Unicode codepoint, like Python's chr()
// TODO: ord(char) — returns INT duck of the Unicode codepoint, like Python's ord()
