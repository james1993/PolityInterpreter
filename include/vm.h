#ifndef polity_vm_h
#define polity_vm_h

#include "chunk.h"
#include "common.h"
#include "table.h"

#define STACK_MAX 256

typedef struct {
    chunk* chunk;
    uint8_t* ip; /* instruction pointer */
    Value stack[STACK_MAX];
    Value* stack_top;
    Table globals;
    Table strings;
    struct Obj* objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} interpret_result;

VM* init_vm();
void free_vm();
interpret_result interpret(VM* vm, char* source);

#endif