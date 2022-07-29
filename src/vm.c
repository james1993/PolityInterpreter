#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

static interpret_result run(VM* vm)
{
    double a, b;
    for(;;) { /* TODO: Infinite loops make me sad :( */

#ifdef DEBUG
        printf("        ");
        for (Value* slot = vm->stack; slot < vm->stack_top; slot++)
            printf("[ %g ]", *slot);
        printf("\n");
        disassemble_instruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = (*vm->ip++)) {
            case OP_CONSTANT:
                Value constant = vm->chunk->constants.values[(*vm->ip++)];
                push(vm, constant);
                break;
            case OP_ADD:
                b = pop(vm); a = pop(vm);
                push(vm, a + b);
                break;
            case OP_SUBTRACT:
                b = pop(vm); a = pop(vm);
                push(vm, a - b);
                break;
            case OP_MULTIPLY:
                b = pop(vm); a = pop(vm);
                push(vm, a * b);
                break;
            case OP_DIVIDE:
                b = pop(vm); a = pop(vm);
                push(vm, a / b);
                break;
            case OP_NEGATE:
                push(vm, -pop(vm));
                break;
            case OP_RETURN:
                printf("%g\n", pop(vm));
                return INTERPRET_OK;
        }
    }
}

VM* init_vm()
{
    VM* vm = (VM*)malloc(sizeof(VM));
    vm->stack_top = vm->stack;

    return vm;
}

interpret_result interpret(VM* vm, const char* source)
{
   compile(source);
   return INTERPRET_OK;
}