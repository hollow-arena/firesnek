// PROMOTION SYSTEM — moves returned RefTypes from callee arena into caller arena
//
// Triggered on any function return where the return type contains a RefType.
// Promotion is one level at a time: callee → caller. If the caller also returns
// that value, a second promotion pass runs at that boundary. Clean abstraction,
// predictable cost per return.
//
// ALGORITHM
//   1. Compiler emits a type descriptor for the return value, encoding which
//      fields are RefTypes and need recursive promotion (value-typed fields are
//      copied in-place trivially).
//   2. Runtime walks the object graph using that descriptor, allocating copies
//      into the caller arena via estus__arena_alloc_copy.
//   3. A hash set tracks visited duck pointers (arena_id, offset pairs) to
//      handle cycles — e.g. a linked list with a back-pointer will not recurse
//      infinitely. Runtime is O(k) where k is reference type depth.
//   4. Hash set is a temporary allocation (heap, not arena) freed immediately
//      after the promotion pass completes.
//      Compiler can hint the initial hash set capacity from the return type's
//      known reference depth, avoiding rehashes for common cases.
//
// CALLING CONVENTION
//   All functions receive an implicit estus__arena *parent_arena as their first
//   parameter, injected by the compiler. This applies universally — not just to
//   functions with known RefType returns — because duck typing means the return
//   tag isn't statically known. On return, the runtime checks the duck tag:
//   primitive tags take the fast path (no promotion), RefType tags trigger the
//   promotion walk into parent_arena. Zero overhead for the common primitive case.
//   This convention also composes cleanly with concurrency: each call frame owns
//   its arena and its parent pointer, no shared state required.
//
// PERFORMANCE NOTE
//   Returning a large object graph has real cost. Savvy users who want to avoid
//   promotion should instantiate the target object in the caller frame and pass
//   it into a void-returning function that mutates in place. The compiler should
//   not try to optimize this automatically — it is a user-visible design choice.
