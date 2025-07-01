#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "object.h"
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
        unsigned char* str = AS_FUNCTION(value)->name->chars;
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    if (IS_CLOSURE(value)) {
        unsigned char* str = AS_CLOSURE(value)->function->name->chars;
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    if (IS_NATIVE(value)) {
        unsigned char* str = "native";
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    if (IS_SET(value)) {
        // This for now
        unsigned char* str = "set";
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    // If its a value
    unsigned char* str;
    switch (value.type) {
        case VAL_BOOL:
            str = BOOL_TO_STRING(AS_BOOL(value));
            break;
        case VAL_NULL:
            str = NULL_TO_STRING;
            break;
        case VAL_NUMBER:
            NUMBER_TO_STRING(AS_NUMBER(value), &str);
            break;
        default: 
            str = "CAST_ERROR";
            // return NULL;
            break;
    }

    ObjString* result = AS_STRING(OBJ_VAL(copyString(str, strlen(str))));

    return result;
}

void printValue(Value value) {
    ObjString* stringRep = valueToString(value);
    printJMPLString(stringRep);
}

bool valuesEqual(Value a, Value b) {
    if(a.type != b.type) return false;

    switch(a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:   return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);

        default: return false;
    }
}