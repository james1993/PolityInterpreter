#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void init_chunk(chunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    init_value_array(&chunk->constants);
}

void write_chunk(chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1) {
        chunk->capacity = GROW_CAPACITY(chunk->capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void free_chunk(chunk* chunk)
{
    free(chunk->code);
    free(chunk->lines);
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

int add_constant(chunk* chunk, Value value)
{
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}