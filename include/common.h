#ifndef polity_common_h
#define polity_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define DEBUG
#define UINT8_COUNT (UINT8_MAX + 1)

#define AS_BOOL(val)      ((val).as.boolean)
#define AS_NUMBER(val)    ((val).as.number)
#define AS_OBJ(val)       ((val).as.obj)
#define AS_STRING(val)    ((obj_string*)AS_OBJ(val))
#define AS_CSTRING(val)   (((obj_string*)AS_OBJ(val))->chars)

#define IS_BOOL(val)      ((val).type == VAL_BOOL)
#define IS_NIL(val)       ((val).type == VAL_NIL)
#define IS_NUMBER(val)    ((val).type == VAL_NUMBER)
#define IS_OBJ(val)       ((val).type == VAL_OBJ)
#define IS_STRING(val)    IS_OBJ(val) && AS_OBJ(val)->type == OBJ_STRING

#define BOOL_VAL(val)     ((value){VAL_BOOL, {.boolean = val}})
#define NIL_VAL             ((value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(val)   ((value){VAL_NUMBER, {.number = val}})
#define OBJ_VAL(object)     ((value){VAL_OBJ, {.obj = (struct Obj*)object}})
#define OBJ_TYPE(val)     (AS_OBJ(val)->type)

typedef enum {
    /* Single-character tokens */
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    /* One or two character tokens */
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    /* Literals */
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    /* Keywords */
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
    TOKEN_ERROR,
    TOKEN_EOF
} token_type;

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_RETURN,
} op_code;

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

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} interpret_result;

typedef enum {
    OBJ_STRING,
} obj_type;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
} value_type;

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

#endif