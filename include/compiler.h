#ifndef polity_compiler_h
#define polity_compiler_h

#include "vm.h"
#include "scanner.h"
#include "chunk.h"

typedef void (*parse_fn)(parser* p, scanner* s, chunk* c);

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

bool compile(const char* source, chunk* ch);

#endif