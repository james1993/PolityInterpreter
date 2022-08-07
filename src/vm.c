#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

static inline void push(VM* vm, Value value) { *(vm->stack_top++) = value; }
static inline Value pop(VM* vm) { return *(--vm->stack_top); }
static inline Value peek(VM* vm, int distance) { return vm->stack_top[-1 - distance]; }
static inline bool is_falsey(Value val) { return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val)); }

static interpret_result runtime_error(VM* vm, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);

    return INTERPRET_RUNTIME_ERROR;
}

static bool values_equal(Value a, Value b)
{
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:
            return AS_STRING(a)->length == AS_STRING(b)->length &&
                    memcmp(AS_STRING(a)->chars, AS_STRING(b)->chars, AS_STRING(a)->length) == 0;
        default:
            return false;
    }
}

static interpret_result check(VM* vm)
{
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
        return runtime_error(vm, "Operands must be numbers");

    return INTERPRET_OK;
}

static void print_value(Value value)
{
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL:
            printf("nil"); break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: 
            switch (OBJ_TYPE(value)) {
                case OBJ_STRING:
                    printf("%s", AS_CSTRING(value));
                    break;
            }
            break;
    }
}

static void concatenate(VM* vm)
{
    obj_string* b = AS_STRING(pop(vm));
    obj_string* a = AS_STRING(pop(vm));

    int length = a->length + b->length;
    char* chars = (char*)malloc(sizeof(char) * (length + 1));
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    uint32_t hash = hash_string(chars, length);
    obj_string* result = allocate_string(vm, chars, length, hash);
    push(vm, OBJ_VAL(result));
}

static interpret_result run(VM* vm)
{
    double a, b;
    uint8_t instruction;

    while (1) {
        switch (instruction = (*vm->ip++)) {
            case OP_CONSTANT:
                Value constant = vm->chunk->constants.values[(*vm->ip++)];
                push(vm, constant);
                break;
            case OP_NIL:
                push(vm, NIL_VAL);
                break;
            case OP_TRUE:
                push(vm, BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(vm, BOOL_VAL(false));
                break;
            case OP_POP:
                pop(vm);
                break;
            case OP_GET_LOCAL:
                push(vm, vm->stack[(*vm->ip++)]);
                break;
            case OP_SET_LOCAL:
                vm->stack[(*vm->ip++)] = peek(vm, 0);
                break;
            case OP_GET_GLOBAL:
                obj_string* global_name = AS_STRING(vm->chunk->constants.values[(*vm->ip++)]);
                Value value;
                if (!table_get(&vm->globals, global_name, &value))
                    return runtime_error(vm, "Undefined variable '%s'", global_name->chars);
                push(vm, value);
                break;
            case OP_DEFINE_GLOBAL:
                obj_string* global_def = AS_STRING(vm->chunk->constants.values[(*vm->ip++)]);
                table_set(&vm->globals, global_def, peek(vm, 0));
                pop(vm);
                break;
            case OP_SET_GLOBAL:
                obj_string* global_set = AS_STRING(vm->chunk->constants.values[(*vm->ip++)]);
                if (table_set(&vm->globals, global_set, peek(vm, 0))) {
                    table_delete(&vm->globals, global_set);
                    return runtime_error(vm, "Undefined variable '%s'", global_set->chars);
                }
                break;
            case OP_EQUAL:
                push(vm, BOOL_VAL(values_equal(pop(vm), pop(vm))));
                break;
            case OP_GREATER:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;
                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));

                push(vm, NUMBER_VAL(a > b));
                break;
            case OP_LESS:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                push(vm, NUMBER_VAL(a < b));
                break;
            case OP_ADD:
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1)))
                    concatenate(vm);
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1)))
                    push(vm, NUMBER_VAL(AS_NUMBER(pop(vm)) + AS_NUMBER(pop(vm))));
                else
                    return runtime_error(vm, "Operands must be two numbers or two strings\n");
                break;
            case OP_SUBTRACT:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a - b));
                break;
            case OP_MULTIPLY:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                push(vm, NUMBER_VAL(AS_NUMBER(pop(vm)) * AS_NUMBER(pop(vm))));
                break;
            case OP_DIVIDE:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a / b));
                break;
            case OP_NOT:
                push(vm, BOOL_VAL(is_falsey(pop(vm))));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0)))
                    return runtime_error(vm, "Operand must be a number");

                push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
                break;
            case OP_PRINT: 
                print_value(pop(vm));
                printf("\n");
                break;
            case OP_JUMP:
                vm->ip += (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]));
                break;
            case OP_JUMP_IF_FALSE:
                if (is_falsey(peek(vm, 0)))
                    vm->ip += (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]));
                break;
            case OP_LOOP:
                vm->ip -= (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]));
                break;
            case OP_RETURN:
                /* Exit interpreter */
                return INTERPRET_OK;
        }
    }
}

VM* init_vm()
{
    VM* vm = (VM*)malloc(sizeof(VM));

    vm->stack_top = vm->stack;
    vm->objects = NULL;
    vm->strings.count = 0;
    vm->strings.capacity = 0;
    vm->strings.entries = NULL;
    vm->globals.count = 0;
    vm->globals.capacity = 0;
    vm->globals.entries = NULL;

    return vm;
}

void free_vm(VM* vm)
{
    /* Free allocated strings */
	struct Obj* object = vm->objects;
	while (object != NULL) {
		struct Obj* next = object->next;
		switch (object->type) {
			case OBJ_STRING:
				obj_string* str = (obj_string*)object;
				free(str->chars);
				free(str);
		}
		object = next;
	}

	/* Free virtual machine */
	free(vm->strings.entries);
	free(vm->globals.entries);
	free(vm);
}

interpret_result interpret(VM* vm, char* source)
{
    chunk* ch = (chunk*)calloc(1, sizeof(chunk));

    if (!compile(source, ch, vm)) {
        free_chunk(ch);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = ch;
    vm->ip = ch->code;

    interpret_result result = run(vm);

    free_chunk(ch);
    return result;
}