#ifndef polity_memory_h
#define polity_memory_h

#include "common.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, new_count) \
    (type*)reallocate(pointer, sizeof(type) * (new_count))

void* reallocate(void* pointer, size_t new_size);

#endif