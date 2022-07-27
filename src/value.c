#include <stdio.h>

#include "memory.h"
#include "value.h"

void print_value(Value value)
{
    printf("%g", value);
}

void init_value_array(value_array* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void write_value_array(value_array* array, Value value)
{
    if (array->capacity < array->count + 1) {
        array->capacity = GROW_CAPACITY(array->capacity);
        array->values = GROW_ARRAY(Value, array->values, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void free_value_array(value_array* array)
{
    free(array->values);
    init_value_array(array);
}