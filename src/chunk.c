#include "chunk.h"
#include "memory.h"
#include "common.h"

void write_chunk(chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1) {
        chunk->capacity = (chunk->capacity) < 8 ? 8 : (chunk->capacity) * 2;
        chunk->code = (uint8_t*)realloc(chunk->code, sizeof(uint8_t) * (chunk->capacity));
        chunk->lines = (int*)realloc(chunk->lines, sizeof(int) * (chunk->capacity));
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void free_chunk(chunk* chunk)
{
    free(chunk->code);
    free(chunk->lines);
    free(chunk->constants.values);
    free(chunk);
}

int add_constant(chunk* chunk, Value value)
{
    if (chunk->constants.capacity < chunk->constants.count + 1) {
        chunk->constants.capacity = (chunk->constants.capacity) < 8 ? 8 : (chunk->constants.capacity) * 2;
        chunk->constants.values = (Value*)realloc(chunk->constants.values, sizeof(Value) * (chunk->constants.capacity));
    }

    chunk->constants.values[chunk->constants.count++] = value;
    return chunk->constants.count - 1;
}