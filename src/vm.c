#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "vm.h"
#include "debug.h"

static interpret_result run(VM* vm)
{
    #define READ_BYTE() (*vm->ip++)
    #define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

    for(;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (Value* slot = vm->stack; slot < vm->stack_top; slot++)
            printf("[ %g ]", *slot);
        printf("\n");
        disassemble_instruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_ADD: {
                double b = pop(vm);
                double a = pop(vm);
                push(vm, a + b);
                break;
            }
            case OP_SUBTRACT: {
                double b = pop(vm);
                double a = pop(vm);
                push(vm, a - b);
                break;
            }
            case OP_MULTIPLY: {
                double b = pop(vm);
                double a = pop(vm);
                push(vm, a * b);
                break;
            }
            case OP_DIVIDE: {
                double b = pop(vm);
                double a = pop(vm);
                push(vm, a / b);
                break;
            }
            case OP_NEGATE: {
                push(vm, -pop(vm));
                break;
            }
            case OP_RETURN: {
                printf("%g\n", pop(vm));
                return INTERPRET_OK;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
}

VM* init_vm()
{
    VM* vm = (VM*)malloc(sizeof(VM));
    vm->stack_top = vm->stack;

    return vm;
}

interpret_result interpret(VM* vm, chunk* chunk)
{
    vm->chunk = chunk;
    vm->ip = chunk->code;
    return run(vm);
}