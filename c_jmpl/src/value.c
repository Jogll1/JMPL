#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "value.h"
#include "object.h"
#include "obj_string.h"
#include "set.h"
#include "tuple.h"
#include "memory.h"
#include "unicode.h"
#include "hash.h"

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
#ifdef NAN_BOXING
    if (IS_OBJ(a) && IS_OBJ(b)) {
        ObjType aType = AS_OBJ(a)->type;
        ObjType bType = AS_OBJ(b)->type;

        if (aType != bType) return false;

        switch(AS_OBJ(a)->type) {
            case OBJ_SET:   return setsEqual(AS_SET(a), AS_SET(b));
            case OBJ_TUPLE: return tuplesEqual(AS_TUPLE(a), AS_TUPLE(b));
            default:        return AS_OBJ(a) == AS_OBJ(b);
        }
    } else {
        if (IS_NUMBER(a) && IS_NUMBER(b)) {
            return AS_NUMBER(a) == AS_NUMBER(b);
        }

        return a == b;
    }
#else
    if(a.type != b.type) return false;

    switch(a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:   return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_CHAR:   return AS_CHAR(a) == AS_CHAR(b);
        case VAL_OBJ:    
            switch(AS_OBJ(a)->type) {
                case OBJ_SET:   return setsEqual(AS_SET(a), AS_SET(b));
                case OBJ_TUPLE: return tuplesEqual(AS_TUPLE(a), AS_TUPLE(b));
                default:        return AS_OBJ(a) == AS_OBJ(b);
            }

        default: return false;
    }
#endif
}

static void charToString(uint32_t charCodePoint, unsigned char* output) {
    size_t numBytes = unicodeToUtf8(charCodePoint, output); // assign the output and numBytes
    assert(numBytes > 0);
    output[numBytes] = '\0'; // Terminate correctly 
}

static void printChar(uint32_t codePoint) {
    unsigned char str[5];
    charToString(codePoint, str);
    printf("%s", str);
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
    if (IS_OBJ(value)) {
        if (IS_STRING(value)) {
            return strdup(AS_CSTRING(value));
        }

        if (IS_FUNCTION(value)) {
            ObjString* name = AS_FUNCTION(value)->name;
            if (name != NULL) {
                return strdup(name->utf8);
            }

            return strdup("<anon>");
        }

        if (IS_CLOSURE(value)) {
            ObjString* name = AS_CLOSURE(value)->function->name;
            if (name != NULL) {
                return strdup(name->utf8);
            }

            return strdup("<anon>");
        }

        if (IS_NATIVE(value)) {
            return strdup("<native>");
        }

        if (IS_SET(value)) {
            return setToString(AS_SET(value));
        }

        if (IS_TUPLE(value)) {
            return tupleToString(AS_TUPLE(value));
        }

        if (IS_ITERATOR(value)) {
            return strdup("<iterator>");
        }
    }

    // If its a value
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        return strdup(BOOL_TO_STRING(AS_BOOL(value)));
    } else if (IS_NULL(value)) {
        return strdup(NULL_TO_STRING);
    } else if (IS_NUMBER(value)) {
        unsigned char* str;
        NUMBER_TO_STRING(AS_NUMBER(value), &str);
        return str;
    } else if (IS_CHAR(value)) {
        unsigned char str[5];
        charToString(AS_CHAR(value), str);
        return strdup(str);
    }
#else
    switch (value.type) {
        case VAL_BOOL:
            return strdup(BOOL_TO_STRING(AS_BOOL(value)));
        case VAL_NULL:
            return strdup(NULL_TO_STRING);
        case VAL_NUMBER:
            unsigned char* numStr;
            NUMBER_TO_STRING(AS_NUMBER(value), &numStr);
            return numStr;
        case VAL_CHAR:
            unsigned char charStr[5];
            charToString(AS_CHAR(value), charStr);
            return strdup(charStr);
        default: 
            break;
    }
#endif

    return strdup("<CAST_ERROR>");
}

void printValue(Value value, bool simple) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        printf(AS_BOOL(value) ? "true" : "false");
    } else if (IS_NULL(value)) {
        printf("null");
    } else if (IS_NUMBER(value)) {
        printf("%g", AS_NUMBER(value));
    } else if (IS_CHAR(value)) {
        printChar(AS_CHAR(value));
    } else if (IS_OBJ(value)) {
        printObject(value, simple);
    }
#else
    switch(value.type) {
        case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NULL:   printf("null"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_CHAR:   printChar(AS_CHAR(value)); break;
        case VAL_OBJ:    printObject(value, simple); break;
        default: return;
    }
#endif
}