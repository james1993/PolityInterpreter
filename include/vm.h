#ifndef polity_vm_h
#define polity_vm_h

#include "chunk.h"
#include "common.h"

#define STACK_MAX 256

typedef struct {
    chunk* chunk;
    uint8_t* ip; /* instruction pointer */
    Value stack[STACK_MAX];
    Value* stack_top;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} interpret_result;

VM* init_vm();
bool values_equal(Value a, Value b);
interpret_result interpret(VM* vm, const char* source);
static inline void push(VM* vm, Value value) { *(vm->stack_top++) = value; }
static inline Value pop(VM* vm) { return *(--vm->stack_top); }
static inline Value peek(VM* vm, int distance) { return vm->stack_top[-1 - distance]; }
static inline bool is_falsey(Value val) { return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val)); }

#endif