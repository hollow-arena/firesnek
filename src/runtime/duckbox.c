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
#include "objects/string.h"
#include "objects/list.h"

// EVERY standard unpack will be either a 1 or a 0 (for truthy vs falsy checks).
// Will also include a string unpack function to make print() calls easy.
// All other binary expressions should be evaluated with internal duck functions.

// Only for direct truthy vs falsy casts, i.e. "if (some_integer)"
bool estus__unpack_truthy(estus__duck duck) {
    switch (_get_type_enum(duck)) {
        case FLOAT:     { double d; memcpy(&d, &duck, 8); return d != 0.0; }
        case INT:
        case UINT:      return (duck & 0x0007FFFFFFFFFFFFULL) != 0;
        case BOOL:      return duck & 1;
        case NONE:
        case _NAN:      return 0;
        // TODO: BIGINT truthy = (value != 0); COMPLEX truthy = (real != 0 || imag != 0)
        // TODO: check len field in metadata for empty string/container once PTR semantics are fleshed out
        case BIGINT:
        case COMPLEX:
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

// TODO: Allow for string representations beyond 64 characters
// TODO: Pass through should also work for STRBYTES
estus__duck estus__unpack_str(estus__registry *registry, estus__arena *arena, estus__duck duck) {
    if (_get_type_enum(duck) == STR) return duck;  // already a string, pass through

    char buf[64];
    int  len = 0;

    switch (_get_type_enum(duck)) {
        case FLOAT: { double d; memcpy(&d, &duck, 8);
                        len = snprintf(buf, sizeof(buf), "%g", d);                              break; }
        case INT:       len = snprintf(buf, sizeof(buf), "%" PRId64, estus__unpacki(duck));     break;
        case UINT:      len = snprintf(buf, sizeof(buf), "%" PRIu64, estus__unpacku(duck));     break;
        case BOOL:      len = snprintf(buf, sizeof(buf), "%s",
                                      estus__unpackb(duck) ? "True" : "False");                 break;
        case NONE:      len = snprintf(buf, sizeof(buf), "None");                               break;
        case _NAN:      len = snprintf(buf, sizeof(buf), "NaN");                                break;
        // TODO: walk arena to print actual contents for these types
        case BIGINT:    len = snprintf(buf, sizeof(buf), "<bigint>");                           break;
        case COMPLEX:   len = snprintf(buf, sizeof(buf), "<complex>");                          break;
        case STRBYTES:  len = snprintf(buf, sizeof(buf), "<bytes>");                            break;
        case LIST:      return estus__list_str(registry, arena, duck);
        case LISTBYTES: len = snprintf(buf, sizeof(buf), "<list>");                             break;
        case TUPLE:     len = snprintf(buf, sizeof(buf), "<tuple>");                            break;
        case SET:       len = snprintf(buf, sizeof(buf), "<set>");                              break;
        case DICT:      len = snprintf(buf, sizeof(buf), "<dict>");                             break;
        case DEQUE:     len = snprintf(buf, sizeof(buf), "<deque>");                            break;
        case FUNC:
        case CLOSURE:   len = snprintf(buf, sizeof(buf), "<function>");                         break;
        case OBJ:       len = snprintf(buf, sizeof(buf), "<object>");                           break;
        default:        len = snprintf(buf, sizeof(buf), "<unknown>");                          break;
    }
    return estus__str_new(registry, arena, buf, (size_t)len);
}

// Arithmetic ops will be generally allowed between different types.
// Promotion priority: Float > Int
// Bool - Always 1 or 0. Arithmetic on two bools always results in an int.
// None will always be interpreted as 0.
// Pointer arithmetic is not supported for v1.

// TODO: Arbitrary precision ints — when an INT/UINT arithmetic result exceeds 2^50-1, promote to BIGINT.
// TODO: Valid strings with digit unicode characters should be castable to int / float.

estus__duck estus__casti(estus__duck duck) {
    switch (_get_type_enum(duck)) {
        case FLOAT:  return estus__packi((int64_t)estus__unpackf(duck));
        case INT:    return duck;
        case UINT:   return estus__packi((int64_t)estus__unpacku(duck));
        case BOOL:   return estus__packi((int64_t)estus__unpackb(duck));
        case NONE:   return estus__packi(0);
        case _NAN:   estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast nan to int");              break;
        case BIGINT: return duck;
        case COMPLEX:
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
        case BOOL:  return estus__packf((double)estus__unpackb(duck));
        case NONE:  return estus__packf(0.0);
        case _NAN:  return duck;
        case BIGINT:
        case COMPLEX:
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
        case BOOL:  return duck;
        case NONE:  return estus__packb(false);
        case _NAN:  return estus__packb(false);
        // TODO: BIGINT/COMPLEX: walk heap object to compare against zero
        // TODO: Empty strings/containers are false, non-empty are true
        case BIGINT:
        case COMPLEX:
        case STR:
        case STRBYTES:
        case LIST: case LISTBYTES: case TUPLE: case SET:
        case DICT: case DEQUE: case FUNC: case CLOSURE: case OBJ:
                    estus__panic_roll(ESTUS_ERR_TYPE, "cannot cast reference type to bool");   break;
    }
    return 0ULL;
}

// Arithmetic ops
// TODO: Add list concatenation
// TODO: string * int repetition

#define ESTUS__ARITH_OP(name, int_expr, float_expr)                                  \
estus__duck estus__##name(estus__duck a, estus__duck b) {                            \
    estus__type_enum ta = _get_type_enum(a);                                         \
    estus__type_enum tb = _get_type_enum(b);                                         \
    if (ta == BOOL || ta == NONE) { a = estus__casti(a); ta = INT; }                 \
    if (tb == BOOL || tb == NONE) { b = estus__casti(b); tb = INT; }                 \
    if (ta == FLOAT || tb == FLOAT) {                                                \
        double fa = (ta == FLOAT) ? estus__unpackf(a) : (double)estus__unpacki(a);  \
        double fb = (tb == FLOAT) ? estus__unpackf(b) : (double)estus__unpacki(b);  \
        return float_expr;                                                           \
    }                                                                                \
    if ((ta == INT || ta == UINT) && (tb == INT || tb == UINT)) {                    \
        int64_t ia = estus__unpacki(a);                                              \
        int64_t ib = estus__unpacki(b);                                              \
        return int_expr;                                                             \
    }                                                                                \
    estus__panic_roll(ESTUS_ERR_TYPE, "unsupported types for " #name);              \
    return estus__packn();                                                           \
}

estus__duck estus__add(estus__registry *registry, estus__arena *arena, estus__duck a, estus__duck b) {
    estus__type_enum ta = _get_type_enum(a);
    estus__type_enum tb = _get_type_enum(b);
    if (ta == BOOL || ta == NONE) { a = estus__casti(a); ta = INT; }
    if (tb == BOOL || tb == NONE) { b = estus__casti(b); tb = INT; }
    if (ta == FLOAT || tb == FLOAT) {
        double fa = (ta == FLOAT) ? estus__unpackf(a) : (double)estus__unpacki(a);
        double fb = (tb == FLOAT) ? estus__unpackf(b) : (double)estus__unpacki(b);
        return estus__packf(fa + fb);
    }
    if ((ta == INT || ta == UINT) && (tb == INT || tb == UINT)) {
        int64_t ia = estus__unpacki(a);
        int64_t ib = estus__unpacki(b);
        return estus__packi(ia + ib);
    }
    if (ta == STR && tb == STR)
        return estus__str_concat(registry, arena, a, b);
    if (ta == LIST && tb == LIST)
        return estus__list_concat(registry, arena, a, b);
    estus__panic_roll(ESTUS_ERR_TYPE, "unsupported types for add");
    return estus__packn();
}

estus__duck estus__mul(estus__registry *registry, estus__arena *arena, estus__duck a, estus__duck b) {
    estus__type_enum ta = _get_type_enum(a);
    estus__type_enum tb = _get_type_enum(b);
    if (ta == BOOL || ta == NONE) { a = estus__casti(a); ta = INT; }
    if (tb == BOOL || tb == NONE) { b = estus__casti(b); tb = INT; }
    if (ta == FLOAT || tb == FLOAT) {
        double fa = (ta == FLOAT) ? estus__unpackf(a) : (double)estus__unpacki(a);
        double fb = (tb == FLOAT) ? estus__unpackf(b) : (double)estus__unpacki(b);
        return estus__packf(fa * fb);
    }
    if ((ta == INT || ta == UINT) && (tb == INT || tb == UINT)) {
        int64_t ia = estus__unpacki(a);
        int64_t ib = estus__unpacki(b);
        return estus__packi(ia * ib);
    }
    if (ta == STR && (tb == INT || tb == UINT)) return estus__str_mult(registry, arena, a, b);
    if (tb == STR && (ta == INT || ta == UINT)) return estus__str_mult(registry, arena, b, a);
    if (ta == LIST && (tb == INT || tb == UINT)) return estus__list_mult(registry, arena, a, b);
    if (tb == LIST && (ta == INT || ta == UINT)) return estus__list_mult(registry, arena, b, a);
    estus__panic_roll(ESTUS_ERR_TYPE, "unsupported types for mul");
    return estus__packn();
}

// clang-format off
ESTUS__ARITH_OP(sub,      estus__packi(ia - ib),                                                      estus__packf(fa - fb))
ESTUS__ARITH_OP(div,      estus__packf((double)ia / (double)ib),                                      estus__packf(fa / fb))
ESTUS__ARITH_OP(floordiv, estus__packi(ia/ib - (ia%ib != 0 && (ia^ib) < 0)),                          estus__packf(floor(fa / fb)))
ESTUS__ARITH_OP(mod,      estus__packi(ia%ib + (ia%ib != 0 && (ia^ib) < 0 ? ib : 0)),                 estus__packf(fa - floor(fa/fb)*fb))
// int**int routes through double pow(); precision is fine since INT ducks are capped at 51 bits
// and doubles have 52-bit significands.
ESTUS__ARITH_OP(pow,      (ib >= 0) ? estus__packi((int64_t)pow(ia, ib)) : estus__packf(pow(ia, ib)), estus__packf(pow(fa, fb)))
// clang-format on

// ─── Bitwise operations ──────────────────────────────────────────────────────
//
// Python raises TypeError for bitwise ops on floats, so there is no float path.
// BOOL and NONE normalise to INT as with arithmetic.
// TODO: validate shift counts (negative → ValueError, >= 64 → UB in C).

#define ESTUS__BITWISE_OP(name, expr)                                                \
estus__duck estus__##name(estus__duck a, estus__duck b) {                            \
    estus__type_enum ta = _get_type_enum(a);                                         \
    estus__type_enum tb = _get_type_enum(b);                                         \
    if (ta == BOOL || ta == NONE) { a = estus__casti(a); ta = INT; }                \
    if (tb == BOOL || tb == NONE) { b = estus__casti(b); tb = INT; }                \
    if ((ta == INT || ta == UINT) && (tb == INT || tb == UINT)) {                    \
        int64_t ia = estus__unpacki(a);                                              \
        int64_t ib = estus__unpacki(b);                                              \
        return expr;                                                                 \
    }                                                                                \
    estus__panic_roll(ESTUS_ERR_TYPE, "unsupported types for " #name);              \
    return estus__packn();                                                           \
}

// clang-format off
ESTUS__BITWISE_OP(band,   estus__packi(ia & ib))
ESTUS__BITWISE_OP(bor,    estus__packi(ia | ib))
ESTUS__BITWISE_OP(bxor,   estus__packi(ia ^ ib))
ESTUS__BITWISE_OP(lshift, estus__packi(ia << ib))
ESTUS__BITWISE_OP(rshift, estus__packi(ia >> ib))
// clang-format on

estus__duck estus__abs(estus__duck a) {
    estus__type_enum ta = _get_type_enum(a);
    if (ta == BOOL)               return a;
    if (ta == INT || ta == UINT)  return a & ~(1ULL << 50);  // clear sign-magnitude sign bit
    if (ta == FLOAT)              return a & ~LARGE_BIT;      // clear IEEE 754 sign bit
    estus__panic_roll(ESTUS_ERR_TYPE, "unsupported type for abs");
    return estus__packn();
}

// Unary ~ — single operand, no macro needed
estus__duck estus__invert(estus__duck a) {
    estus__type_enum ta = _get_type_enum(a);
    if (ta == BOOL || ta == NONE) { a = estus__casti(a); ta = INT; }
    if (ta == INT || ta == UINT) return estus__packi(~estus__unpacki(a));
    estus__panic_roll(ESTUS_ERR_TYPE, "unsupported type for invert");
    return estus__packn();
}

// Comparison functions
// TODO: Add strings / objects as appropriate, just doing numbers for now
#define ESTUS__CMPR_OP(name, int_expr, float_expr)                                   \
estus__duck estus__##name(estus__duck a, estus__duck b) {                            \
    estus__type_enum ta = _get_type_enum(a);                                         \
    estus__type_enum tb = _get_type_enum(b);                                         \
    if (ta == BOOL || ta == NONE) { a = estus__casti(a); ta = INT; }                 \
    if (tb == BOOL || tb == NONE) { b = estus__casti(b); tb = INT; }                 \
    if (ta == FLOAT || tb == FLOAT) {                                                \
        double fa = (ta == FLOAT) ? estus__unpackf(a) : (double)estus__unpacki(a);  \
        double fb = (tb == FLOAT) ? estus__unpackf(b) : (double)estus__unpacki(b);  \
        return estus__packb(float_expr);                                             \
    }                                                                                \
    if ((ta == INT || ta == UINT) && (tb == INT || tb == UINT)) {                    \
        int64_t ia = estus__unpacki(a);                                              \
        int64_t ib = estus__unpacki(b);                                              \
        return estus__packb(int_expr);                                               \
    }                                                                                \
    estus__panic_roll(ESTUS_ERR_TYPE, "unsupported types for " #name);              \
    return estus__packn();                                                           \
}

// clang-format off
ESTUS__CMPR_OP(eq,   ia == ib, fa == fb)
ESTUS__CMPR_OP(noteq,ia != ib, fa != fb)
ESTUS__CMPR_OP(lt,   ia <  ib, fa <  fb)
ESTUS__CMPR_OP(lte,  ia <= ib, fa <= fb)
ESTUS__CMPR_OP(gt,   ia >  ib, fa >  fb)
ESTUS__CMPR_OP(gte,  ia >= ib, fa >= fb)
// clang-format on

estus__duck estus__index(estus__registry *registry, estus__duck obj, estus__duck idx) {
    estus__type_enum ti = _get_type_enum(idx);
    if (ti != INT && ti != UINT) {
        estus__panic_roll(ESTUS_ERR_INDEX, "index must be valid integer");
        return 0;
    }
    switch (_get_type_enum(obj)) {
        case STR:  return estus__str_index_value(registry, obj, idx);
        case LIST: return estus__list_index_value(registry, obj, idx);
        default:   estus__panic_roll(ESTUS_ERR_TYPE, "type is not subscriptable");
    }
    return estus__packn();
}

// TODO: Add rest of len API once structures fleshed out
estus__duck estus__len(estus__registry *registry, estus__duck duck) {
    switch (_get_type_enum(duck)) {
        case STR:   return estus__str_len(registry, duck);
        case STRBYTES:
        case LIST:  return estus__list_len(registry, duck);
        case LISTBYTES:
        case TUPLE:
        case SET:
        case DICT:
        case DEQUE:
        default: estus__panic_roll(ESTUS_ERR_TYPE, "Invalid type for len");
    }
}