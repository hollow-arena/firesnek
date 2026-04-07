#pragma once
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    ESTUS_ERR_GENERIC,
    ESTUS_ERR_TYPE,
    ESTUS_ERR_VALUE,
    ESTUS_ERR_INDEX,
    ESTUS_ERR_KEY,
    ESTUS_ERR_NULL,
    ESTUS_ERR_SYNTAX,
    ESTUS_ERR_MEMORY,
    ESTUS_ERR_NOTIMPLEMENTED,
} estus__error_kind;

static const char *estus__err_names[] = {
    "RuntimeError",
    "TypeError",
    "ValueError",
    "IndexError",
    "KeyError",
    "NullError",
    "SyntaxError",
    "MemoryError",
    "NotImplementedError",
};

static inline void estus__panic_roll(estus__error_kind kind, const char *msg) {
    fprintf(stderr, "%s: %s\n", estus__err_names[kind], msg);
    fprintf(stderr, "You died.\n");
    exit(1);
}