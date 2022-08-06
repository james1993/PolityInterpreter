#ifndef polity_compiler_h
#define polity_compiler_h

#include "vm.h"
#include "scanner.h"
#include "chunk.h"

typedef struct {
    token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} compiler;

typedef void (*parse_fn)(VM* vm, parser* p, scanner* s, chunk* c, compiler* comp, bool can_assign);

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} precedence;

typedef struct {
    parse_fn prefix;
    parse_fn infix;
    precedence prec;
} parse_rule;

bool compile(char* source, chunk* ch, VM* vm);
obj_string* allocate_string(VM* vm, char* chars, int length, uint32_t hash);
uint32_t hash_string(const char* key, int length);

#endif