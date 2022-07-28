#ifndef polity_common_h
#define polity_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_TRACE_EXECUTION

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value* values;
} value_array;

#endif