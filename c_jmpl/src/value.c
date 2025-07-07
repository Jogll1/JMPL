#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
#include "memory.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if(array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;

        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

/**
 * @brief Converts a value to an ObjString.
 * 
 * @param value The value to convert
 * @return      A pointer to an ObjString
 */
ObjString* valueToString(Value value) {
    // If its an object
    if (IS_STRING(value)) {
        return AS_STRING(value);
    }

    if (IS_FUNCTION(value)) {
        return AS_FUNCTION(value)->name;
    }

    if (IS_CLOSURE(value)) {
        return AS_CLOSURE(value)->function->name;
    }

    if (IS_NATIVE(value)) {
        unsigned char* str = "native";
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    if (IS_SET(value)) {
        unsigned char* str = setToString(AS_SET(value));
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    if (IS_TUPLE(value)) {
        unsigned char* str = tupleToString(AS_TUPLE(value));
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    if (IS_SET_ITERATOR(value)) {
        unsigned char* str = "iterator";
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    // If its a value
    unsigned char* str;
    bool shouldFree = false;
    switch (value.type) {
        case VAL_BOOL:
            str = BOOL_TO_STRING(AS_BOOL(value));
            break;
        case VAL_NULL:
            str = NULL_TO_STRING;
            break;
        case VAL_NUMBER:
            NUMBER_TO_STRING(AS_NUMBER(value), &str);
            shouldFree = true;
            break;
        default: 
            str = "CAST_ERROR";
            break;
    }

    ObjString* result = AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    if (shouldFree) free(str);
    
    return result;
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