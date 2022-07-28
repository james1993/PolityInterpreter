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
interpret_result interpret(VM* vm, chunk* chunk);
static inline void push(VM* vm, Value value) { *(vm->stack_top++) = value; }
static inline Value pop(VM* vm) { return *(--vm->stack_top); }

#endif