#ifndef DUCKBOX_H
#define DUCKBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ─── Architecture configuration ─────────────────────────────────────────────
//
// TODO: The duck box is currently hardcoded for 64-bit NaN-boxing (IEEE 754
// double as the carrier word).  Before supporting other targets, define a
// ESTUS_DUCK_ARCH macro (via build system / -D flag) and gate the sections
// below behind it.  Rough sketch of the three target tiers:
//
//   ESTUS_DUCK_ARCH_32  — embedded / microcontrollers
//       • estus__duck  → uint32_t
//       • Carrier: 32-bit float (IEEE 754 single) or a tag-in-high-bits
//         scheme (e.g. 2 tag bits + 30-bit payload).
//       • Inline string capacity drops from 6 bytes to ~2.
//       • INT range shrinks to ~28 bits signed; bigint threshold moves lower.
//       • Pointer width: 32 bits; packp/unpackp masks change accordingly.
//       • _Static_assert: sizeof(estus__duck) == 4.
//
//   ESTUS_DUCK_ARCH_64  — default (current implementation)
//       • estus__duck  → uint64_t  (this file as written)
//       • Canonical pointer space: 48-bit (AMD64 / AArch64 user-space).
//       • For Linux with 5-level paging (CONFIG_X86_5LEVEL, 57-bit VA):
//           - Under the arena-relative scheme this is less of a concern —
//             PTR ducks never store raw addresses. But PTR_B_MAP tag bits
//             48-56 must still not collide with anything the OS uses in
//             that range when interpreting the 48-bit payload as an index+
//             offset pair. Audit required.
//           - Consider a dedicated ESTUS_DUCK_ARCH_64_LA57 sub-flag.
//
//   ESTUS_DUCK_ARCH_128 — opt-in for scientific / HPC builds
//       • estus__duck  → unsigned __int128  (GCC/Clang extension)
//       • Carrier: 64-bit double in the low 64 bits; high 64 bits hold
//         extended tag + payload (e.g. full 64-bit pointers, 64-bit ints
//         without sign-magnitude tricks, inline strings up to ~14 bytes).
//       • Arithmetic helpers (negatei, negatef, …) and all masks need
//         128-bit equivalents.
//       • _Static_assert: sizeof(estus__duck) == 16 and __SIZEOF_INT128__
//         is defined.
//       • Alignment: arenas / containers must align to 16 bytes.
//
// ─── NaN-boxing bit masks ───────────────────────────────────────────────────

#define FULL_EXP  0x7FF0000000000000ULL // exponent all-ones  → not a float
#define FULL_MANT 0x000FFFFFFFFFFFFFULL // mantissa bits
#define INT_FLAG  0x0008000000000000ULL // bit 51 set         → INT or UINT
#define LARGE_BIT 0x8000000000000000ULL // bit 63             → sign / large flag
#define PTR_B_MAP 0x7FF7000000000000ULL // tag pattern for PTR / BOOL
#define NEG_INT64 0xFFF8000000000000ULL // upper bits to strip from INT/UINT duck

// ─── Core types ─────────────────────────────────────────────────────────────

typedef uint64_t estus__duck;
typedef uint8_t  byte;

// Tag bit patterns: bits 0, 13-16 (left-to-right naming)
// PTR values are canonical 48-bit addresses (user-space or kernel-space)
typedef enum {
    FLOAT,       // normal IEEE 754 double (exponent not all-ones, or mantissa zero)
    NONE,        // 0, 0000  — bit 63 = 0, set bit 62 for "empty" for containers
    _NAN,        // 1, 0000  — bit 63 = 1
    BIGINT,      // 0, 0001  — Stores pointer value
    COMPLEX,     // 1, 0001  — Stores pointer value
    STR,         // 0, 0010  — Stores pointer value
    STRBYTES,    // 1, 0010  — Stores pointer value
    LIST,        // 0, 0011  — Stores pointer value
    LISTBYTES,   // 1, 0011  — Stores pointer value
    TUPLE,       // 0, 0100  — Stores pointer value
    SET,         // 1, 0100  — Stores pointer value
    DICT,        // 0, 0101  — Stores pointer value
    DEQUE,       // 1, 0101  — Stores pointer value
    FUNC,        // 0, 0110  — Stores pointer value
    CLOSURE,     // 1, 0110  — Stores pointer value
    OBJ,         // 0, 0111  — Stores pointer value
    BOOL,        // 1, 0111  — bit 63 = 1 for true, 0 for false
    INT,         // 0, 1xxx  — sign-magnitude, 51 bits
    UINT,        // 1, 1xxx  — unsigned,       51 bits
} estus__type_enum;

// ─── Architecture assertions ─────────────────────────────────────────────────

#ifdef __GNUC__
_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "estus duck box requires little-endian architecture");
#endif

// ─── Type dispatch ───────────────────────────────────────────────────────────

static inline estus__type_enum _get_type_enum(estus__duck duck) {
    bool is_float = !!((duck & FULL_EXP) ^ FULL_EXP) | !(duck & FULL_MANT);
    bool is_int   =   duck & INT_FLAG;
    //     0 for float   8 if int        0 if int     get bit tag value          get most significant bit
    return !is_float * ((is_int << 4) + (!is_int * (((duck >> 48) & 7) << 1)) + (duck >> 63) + 1);
}

// ─── Pack ────────────────────────────────────────────────────────────────────

// INT layout: [63]=sign [62:52]=exponent-ones [51]=INT_FLAG [50]=sign-mag-sign [49:0]=magnitude
static inline estus__duck estus__packi(int64_t i) {
    uint64_t sign = (uint64_t)i >> 63;
    uint64_t mag  = (i ^ -(int64_t)sign) + sign;   // branchless abs
    return FULL_EXP | INT_FLAG | (sign << 50) | (mag & 0x0003FFFFFFFFFFFFULL);
}

// UINT: always positive, wraps at 51 bits (used for FFI / RustTypes; not in v1 surface language)
static inline estus__duck estus__packu(uint64_t u) {
    return u | FULL_EXP | INT_FLAG | LARGE_BIT;
}

// FLOAT: direct bit copy; input NaN is normalised to a canonical quiet-NaN duck
static inline estus__duck estus__packf(double d) {
    estus__duck duck;
    memcpy(&duck, &d, sizeof(double));
    bool is_nan = (duck & FULL_EXP) == FULL_EXP && (duck & FULL_MANT);
    return is_nan ? UINT64_MAX ^ 15ULL : duck;
}

// ─── Heap reference pack ─────────────────────────────────────────────────────
//
// All heap reference ducks share the same bit layout:
//   bits 62:52  FULL_EXP  — NaN tag (marks this as a non-float)
//   bit  51     0         — INT_FLAG not set
//   bits 50:48  type tag  — 3-bit group (001–111, see estus__type_enum)
//   bit  63     variant   — selects the paired type (e.g. LIST vs LISTBYTES)
//   bits 47:32  arena_id  — 16-bit registry index
//   bits 31:0   offset    — 32-bit duck-unit offset into arena
//
// estus__unpackp decodes arena_id + offset identically for all heap types —
// no per-type unpack function needed.

#define ESTUS__PACK_REF(name, tag, variant)                                    \
    static inline estus__duck estus__pack##name(uint16_t arena_id,             \
                                                uint32_t offset) {             \
        return FULL_EXP                                                        \
             | ((uint64_t)(tag)     << 48)                                     \
             | ((uint64_t)arena_id  << 32)                                     \
             | (uint64_t)offset                                                \
             | ((uint64_t)(variant) << 63);                                    \
    }

ESTUS__PACK_REF(bigint,   1, 0)  // BIGINT    — arbitrary-precision integer
ESTUS__PACK_REF(complex,  1, 1)  // COMPLEX   — complex number (real + imag pair)
ESTUS__PACK_REF(str,      2, 0)  // STR       — heap string (chars)
ESTUS__PACK_REF(strbytes, 2, 1)  // STRBYTES  — heap string (raw bytes)
ESTUS__PACK_REF(list,     3, 0)  // LIST
ESTUS__PACK_REF(listbytes,3, 1)  // LISTBYTES — byte array
ESTUS__PACK_REF(tuple,    4, 0)  // TUPLE
ESTUS__PACK_REF(set,      4, 1)  // SET
ESTUS__PACK_REF(dict,     5, 0)  // DICT
ESTUS__PACK_REF(deque,    5, 1)  // DEQUE
ESTUS__PACK_REF(func,     6, 0)  // FUNC
ESTUS__PACK_REF(closure,  6, 1)  // CLOSURE
ESTUS__PACK_REF(obj,      7, 0)  // OBJ — generic user-defined class instance

// Generic heap pointer — use the typed packers above where the type is known.
// Kept for cases where the type tag is computed dynamically at runtime.
static inline estus__duck estus__packp(uint16_t arena_id, uint32_t offset,
                                       estus__type_enum type) {
    uint8_t tag     = ((uint8_t)(type - 1) >> 1) & 7;  // recover 3-bit group from enum value
    uint8_t variant =  (uint8_t)(type - 1) & 1;        // recover variant bit
    return FULL_EXP
         | ((uint64_t)tag      << 48)
         | ((uint64_t)arena_id << 32)
         | (uint64_t)offset
         | ((uint64_t)variant  << 63);
}

// BOOL: PTR_B_MAP | LARGE_BIT | (0 or 1)
static inline estus__duck estus__packb(bool b) {
    return PTR_B_MAP | b | LARGE_BIT;
}

// NONE: FULL_EXP | 1  (mantissa non-zero, bit 63 = 0, tag bits = 0000)
static inline estus__duck estus__packn(void) {
    return FULL_EXP | 1;
}

// ─── Unpack ──────────────────────────────────────────────────────────────────

// INT: decode sign-magnitude back to two's complement
static inline int64_t estus__unpacki(estus__duck duck) {
    uint64_t sign = (duck >> 50) & 1;
    uint64_t mag  = duck & 0x0003FFFFFFFFFFFFULL;
    return (int64_t)((mag ^ -sign) + sign);
}

static inline uint64_t estus__unpacku(estus__duck duck) {
    return duck & ~NEG_INT64;
}

static inline double estus__unpackf(estus__duck duck) {
    double result;
    memcpy(&result, &duck, 8);
    return result;
}

static inline bool estus__unpackb(estus__duck duck) {
    return (duck & 1);
}

// ─── Arithmetic helpers ──────────────────────────────────────────────────────

// Negate a packed INT duck (re-packs through sign-magnitude, handles INT_MIN safely)
static inline estus__duck estus__negatei(estus__duck duck) {
    return estus__packi(-estus__unpacki(duck));
}

// Negate a packed FLOAT duck — flips the sign bit directly, no decode needed
static inline estus__duck estus__negatef(estus__duck duck) {
    return duck ^ LARGE_BIT;
}

// ─── Forward declarations (full definitions in arena.h) ──────────────────────

struct estus__arena_s;
typedef struct estus__arena_s    estus__arena;
struct estus__registry_s;
typedef struct estus__registry_s estus__registry;

// ─── Non-inline public API (defined in duckbox.c) ────────────────────────────

bool          estus__unpack_truthy(estus__duck duck);
estus__duck   estus__unpack_str(estus__registry *registry, estus__arena *arena, estus__duck duck);

estus__duck   estus__casti(estus__duck duck);
estus__duck   estus__castf(estus__duck duck);
estus__duck   estus__castb(estus__duck duck);

estus__duck   estus__add(estus__registry *registry, estus__arena *arena, estus__duck a, estus__duck b);
estus__duck   estus__sub(estus__duck a, estus__duck b);
estus__duck   estus__mul(estus__registry *registry, estus__arena *arena, estus__duck a, estus__duck b);
estus__duck   estus__div(estus__duck a, estus__duck b);
estus__duck   estus__floordiv(estus__duck a, estus__duck b);
estus__duck   estus__mod(estus__duck a, estus__duck b);
estus__duck   estus__pow(estus__duck a, estus__duck b);

estus__duck   estus__band(estus__duck a, estus__duck b);
estus__duck   estus__bor(estus__duck a, estus__duck b);
estus__duck   estus__bxor(estus__duck a, estus__duck b);
estus__duck   estus__lshift(estus__duck a, estus__duck b);
estus__duck   estus__rshift(estus__duck a, estus__duck b);
estus__duck   estus__invert(estus__duck a);
estus__duck   estus__abs(estus__duck a);

estus__duck   estus__eq(estus__duck a, estus__duck b);
estus__duck   estus__noteq(estus__duck a, estus__duck b);
estus__duck   estus__lt(estus__duck a, estus__duck b);
estus__duck   estus__lte(estus__duck a, estus__duck b);
estus__duck   estus__gt(estus__duck a, estus__duck b);
estus__duck   estus__gte(estus__duck a, estus__duck b);

estus__duck   estus__index    (estus__registry *registry, estus__duck obj, estus__duck idx);
estus__duck*  estus__index_ptr(estus__registry *registry, estus__duck obj, estus__duck idx);
estus__duck   estus__len(estus__registry *registry, estus__duck duck);
#endif // DUCKBOX_H