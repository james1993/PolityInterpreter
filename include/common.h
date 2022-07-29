#ifndef polity_common_h
#define polity_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEBUG

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value* values;
} value_array;

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

#endif