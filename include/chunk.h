#ifndef polity_chunk_h
#define polity_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_RETURN,
} op_code;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    value_array constants;
} chunk;

void init_chunk(chunk* chunk);
void write_chunk(chunk* chunk, uint8_t byte, int line);
void free_chunk(chunk* chunk);
int add_constant(chunk* chunk, Value value);

#endif