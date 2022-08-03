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

typedef enum {
    OBJ_STRING,
} obj_type;

struct Obj {
    obj_type type;
    struct Obj* next;
};

typedef struct {
    struct Obj obj;
    int length;
    char* chars;
    uint32_t hash;
} obj_string;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
} value_type;

typedef struct {
    value_type type;
    union {
        bool boolean;
        double number;
        struct Obj* obj;
    } as;
} Value;

#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJ(value)       ((value).as.obj)
#define AS_STRING(value)    ((obj_string*)AS_OBJ(value))
#define AS_CSTRING(value)   (((obj_string*)AS_OBJ(value))->chars)

static inline bool is_obj_type(Value value, obj_type type) { return IS_OBJ(value) && AS_OBJ(value)->type == type; }

#define IS_STRING(value)    is_obj_type(value, OBJ_STRING)

#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (struct Obj*)object}})

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

typedef struct {
    int capacity;
    int count;
    Value* values;
} value_array;

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

#endif