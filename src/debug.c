#include <stdio.h>

#include "debug.h"

int disassemble_instruction(chunk* chunk, int offset)
{
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) printf("   | ");
    else printf("%4d ", chunk->lines[offset]);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            uint8_t constant = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_CONSTANT", constant, chunk->constants.values[constant]);
            return offset + 2;
        case OP_ADD:
            printf("OP_ADD\n");
            return offset + 1;
        case OP_SUBTRACT:
            printf("OP_SUBTRACT\n");
            return offset + 1;
        case OP_MULTIPLY:
            printf("OP_MULTIPLY\n");
            return offset + 1;
        case OP_DIVIDE:
            printf("OP_DIVIDE\n");
            return offset + 1;
        case OP_NEGATE:
            printf("OP_NEGATE\n");
            return offset + 1;
        case OP_RETURN:
            printf("OP_RETURN\n");
            return offset + 1;
        default:
            printf("Unknown opcode (%d)\n", instruction);
    }

    return offset + 1;
}

void disassemble_chunk(chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}