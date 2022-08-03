#ifndef polity_table_h
#define polity_table_h

#include "common.h"

#define TABLE_MAX_LOAD 0.75

typedef struct {
    obj_string* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

bool table_get(Table* table, obj_string* key, Value* value);
bool table_set(Table* table, obj_string* key, Value value);
bool table_delete(Table* table, obj_string* key);
void table_add_all(Table* from, Table* to);
obj_string* table_find_string(Table* table, const char* chars, int length, uint32_t hash);

#endif