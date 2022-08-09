#ifndef polity_interpreter_h
#define polity_interpreter_h

#include "common.h"

#define STACK_MAX 256
#define UINT8_COUNT (UINT8_MAX + 1)
#define TABLE_MAX_LOAD 0.75

typedef struct {
    char* start;
    char* current;
    int line;
} scanner;

typedef struct {
    token_type type;
    char* start;
    int length;
    int line;
} token;

typedef struct {
    token current;
    token previous;
    bool had_error;
    bool panic_mode;
} parser;

typedef struct {
    token name;
    int depth;
} local;

typedef struct {
    local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} compiler;

struct obj {
    obj_type type;
    struct obj* next;
};

typedef struct {
    struct obj obj;
    int arity;
    chunk chunk;
    obj_string* name;
} obj_function;

typedef struct {
    struct obj obj;
    int length;
    char* chars;
    uint32_t hash;
} obj_string;

typedef struct {
    value_type type;
    union {
        bool boolean;
        double number;
        struct obj* obj;
    } as;
} value;

typedef struct {
    int capacity;
    int count;
    value* values;
} value_array;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    value_array constants;
} chunk;

typedef struct {
    obj_string* key;
    value value;
} entry;

typedef struct {
    int count;
    int capacity;
    entry* entries;
} table;

typedef struct {
    chunk* chunk;
    uint8_t* ip; /* instruction pointer */
    value stack[STACK_MAX];
    value* stack_top;
    table globals;
    table strings;
    struct obj* objects;
} VM;

typedef struct {
	VM* vm;
    chunk* chunk;
    scanner* scanner;
    compiler* compiler;
    parser* parser;
    bool can_assign;
} polity_interpreter;

typedef void (*parse_fn)(polity_interpreter* interpreter);

typedef struct {
    parse_fn prefix;
    parse_fn infix;
    precedence prec;
} parse_rule;

VM* init_vm();
void free_vm();
interpret_result interpret(polity_interpreter* interpreter, char* source);
void disassemble_chunk(chunk* chunk, const char* name);
int disassemble_instruction(chunk* chunk, int offset);
scanner* init_scanner(char* source);
token scan_token(scanner* s);
bool compile(char* source, polity_interpreter* interpreter);
obj_string* allocate_string(VM* vm, char* chars, int length, uint32_t hash);
uint32_t hash_string(const char* key, int length);
bool table_get(table* table, obj_string* key, value* value);
bool table_set(table* table, obj_string* key, value value);
bool table_delete(table* table, obj_string* key);
obj_string* table_find_string(table* table, const char* chars, int length, uint32_t hash);
void write_chunk(chunk* chunk, uint8_t byte, int line);
void free_chunk(chunk* chunk);
int add_constant(chunk* chunk, value value);
obj_function* new_function();

#endif