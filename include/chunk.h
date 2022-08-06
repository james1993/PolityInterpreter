#ifndef polity_chunk_h
#define polity_chunk_h

#include "common.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_RETURN,
} op_code;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    value_array constants;
} chunk;

void write_chunk(chunk* chunk, uint8_t byte, int line);
void free_chunk(chunk* chunk);
int add_constant(chunk* chunk, Value value);

#endif