#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
#include "memory.h"
#include "table.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(GC* gc, ValueArray* array, Value value) {
    if(array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;

        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(gc, Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(GC* gc, ValueArray* array) {
    FREE_ARRAY(gc, Value, array->values, array->capacity);
    initValueArray(array);
}

int findInValueArray(ValueArray* array, Value value) {
    for (int i = 0; i < array->count; i++) {
        if (valuesEqual(value, array->values[i])) {
            return i;
        }
    }

    return -1;
}

bool valuesEqual(Value a, Value b) {
    if(a.type != b.type) return false;

    switch(a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:   return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:    
            switch(AS_OBJ(a)->type) {
                case OBJ_SET:   return setsEqual(AS_SET(a), AS_SET(b));
                case OBJ_TUPLE: return tuplesEqual(AS_TUPLE(a), AS_TUPLE(b));
                default:        return AS_OBJ(a) == AS_OBJ(b);
            }

        default: return false;
    }
}

/**
 * @brief Converts a value to an array of chars.
 * 
 * @param value The value to convert
 * @return      A pointer to an array of chars
 * 
 * Must be freed.
 */
unsigned char* valueToString(Value value) {
    // If its an object
    if (IS_STRING(value)) {
        return strdup(AS_STRING(value)->chars);
    }

    if (IS_FUNCTION(value)) {
        return strdup(AS_FUNCTION(value)->name->chars);
    }

    if (IS_CLOSURE(value)) {
        return strdup(AS_CLOSURE(value)->function->name->chars);
    }

    if (IS_NATIVE(value)) {
        return strdup("native");
    }

    if (IS_SET(value)) {
        // Must be freed
        return setToString(AS_SET(value));
    }

    if (IS_TUPLE(value)) {
        // Must be freed
        return tupleToString(AS_TUPLE(value));
    }

    if (IS_SET_ITERATOR(value)) {
        return strdup("iterator");
    }

    // If its a value
    switch (value.type) {
        case VAL_BOOL:
            return strdup(BOOL_TO_STRING(AS_BOOL(value)));
        case VAL_NULL:
            return strdup(NULL_TO_STRING);
        case VAL_NUMBER: {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%.14g", AS_NUMBER(value));
            return strdup(buffer);
        }
        default: 
            return "CAST_ERROR";
    }
}

uint32_t hashValue(Value value) {
    switch(value.type) {
        case VAL_BOOL: return AS_BOOL(value) ? 0xAAAA : 0xBBBB;
        case VAL_NULL: return 0xCCCC;
        case VAL_NUMBER: {
            uint64_t bits = *(uint64_t*)&value.as.number;
            return (uint32_t)(bits ^ (bits >> 32));
        }
        case VAL_OBJ:  
            switch(AS_OBJ(value)->type) {
                case OBJ_SET:   return hashSet(AS_SET(value));
                case OBJ_TUPLE: return hashTuple(AS_TUPLE(value));
                default:        return (uint32_t)((uintptr_t)AS_OBJ(value) >> 2);
            }
        default: return 0;
    }
}

void printValue(Value value) {
    switch(value.type) {
        case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NULL:   printf("null"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ:    printObject(value); break;
        default: return;
    }
}