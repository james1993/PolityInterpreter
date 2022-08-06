#include "common.h"
#include "debug.h"

static int jump_instruction(const char* name, int sign, chunk* chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassemble_instruction(chunk* chunk, int offset)
{
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) printf("   | ");
    else printf("%4d ", chunk->lines[offset]);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            uint8_t constant = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_CONSTANT", constant, AS_NUMBER(chunk->constants.values[constant]));
            return offset + 2;
        case OP_NIL:
            printf("OP_NIL\n");
            return offset + 1;
        case OP_TRUE:
            printf("OP_TRUE\n");
            return offset + 1;
        case OP_FALSE:
            printf("OP_FALSE\n");
            return offset + 1;
        case OP_POP:
            printf("OP_POP");
            return offset + 1;
        case OP_GET_LOCAL:
            uint8_t local_get = chunk->code[offset + 1];
            printf("%-16s %4d\n", "OP_GET_LOCAL", local_get);
            return offset + 2;
        case OP_SET_LOCAL:
            uint8_t local_set = chunk->code[offset + 1];
            printf("%-16s %4d\n", "OP_SET_LOCAL", local_set);
            return offset + 2;
        case OP_GET_GLOBAL:
            uint8_t global_get = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_GET_GLOBAL", global_get, AS_NUMBER(chunk->constants.values[global_get]));
            return offset + 2;
        case OP_DEFINE_GLOBAL:
            uint8_t global_def = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_DEFINE_GLOBAL", global_def, AS_NUMBER(chunk->constants.values[global_def]));
            return offset + 2;
        case OP_SET_GLOBAL:
            uint8_t global_set = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_SET_GLOBAL", global_set, AS_NUMBER(chunk->constants.values[global_set]));
            return offset + 2;
        case OP_EQUAL:
            printf("OP_EQUAL\n");
            return offset + 1;
        case OP_GREATER:
            printf("OP_GREATER\n");
            return offset + 1;
        case OP_LESS:
            printf("OP_LESS\n");
            return offset + 1;
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
        case OP_NOT:
            printf("OP_NOT");
            return offset + 1;
        case OP_NEGATE:
            printf("OP_NEGATE\n");
            return offset + 1;
        case OP_PRINT:
            printf("OP_PRINT\n");
            return offset + 1;
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);
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