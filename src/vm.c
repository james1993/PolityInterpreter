#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

static void runtime_error(VM* vm, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
}

static interpret_result check(VM* vm)
{
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
        runtime_error(vm, "Operands must be numbers");
        return INTERPRET_RUNTIME_ERROR;
    }

    return INTERPRET_OK;
}

static void print_value(Value value)
{
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    }
}

bool values_equal(Value a, Value b)
{
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:       return true;
        case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
        default:
            return false;
    }
}

static interpret_result run(VM* vm)
{
    double a, b;
    interpret_result result = INTERPRET_OK;
    for(;;) {

#ifdef DEBUG
        printf("        ");
        for (Value* slot = vm->stack; slot < vm->stack_top; slot++)
            print_value(*slot);
        printf("\n");
        disassemble_instruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = (*vm->ip++)) {
            case OP_CONSTANT:
                Value constant = vm->chunk->constants.values[(*vm->ip++)];
                push(vm, constant);
                break;
            case OP_NIL: push(vm, NIL_VAL); break;
            case OP_TRUE: push(vm, BOOL_VAL(true)); break;
            case OP_FALSE: push(vm, BOOL_VAL(false));
            case OP_EQUAL:
                push(vm, BOOL_VAL(values_equal(pop(vm), pop(vm))));
                break;
            case OP_GREATER:
                if (check(vm)) return check(vm);
                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a > b));
                break;
            case OP_LESS:
                if (check(vm)) return check(vm);
                push(vm, NUMBER_VAL(a < b));
                break;
            case OP_ADD:
                if (check(vm)) return check(vm);
                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a + b));
                break;
            case OP_SUBTRACT:
                if (check(vm)) return check(vm);
                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a - b));
                break;
            case OP_MULTIPLY:
                if (check(vm)) return check(vm);
                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a * b));
                break;
            case OP_DIVIDE:
                if (check(vm)) return check(vm);
                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a / b));
                break;
            case OP_NOT:
                push(vm, BOOL_VAL(is_falsey(pop(vm))));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtime_error(vm, "Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
                break;
            case OP_RETURN:
                print_value(pop(vm));
                printf("\n");
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
    chunk* ch = (chunk*)calloc(1, sizeof(chunk));

    if (!compile(source, ch)) {
        free_chunk(ch);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = ch;
    vm->ip = ch->code;

    interpret_result result = run(vm);

    free_chunk(ch);
    return result;
}