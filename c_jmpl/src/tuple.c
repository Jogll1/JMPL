#include <stdlib.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "tuple.h"
#include "gc.h"
#include "utils.h"
#include "../lib/c-stringbuilder/sb.h"

ObjTuple* newTuple(GC* gc, size_t size) {
    ObjTuple* tuple = ALLOCATE_OBJ(gc, ObjTuple, OBJ_TUPLE, true);
    tuple->size = size;
    tuple->elements = ALLOCATE(gc, Value, size); 
    
    for (size_t i = 0; i < size; i++) {
        tuple->elements[i] = NULL_VAL;
    }
    
    return tuple;
}

bool tuplesEqual(ObjTuple* a, ObjTuple* b) {
    if (a->size != b->size) return false;
    
    for (size_t i = 0; i < a->size; i++) {
        Value valA = a->elements[i];

        for (size_t j = 0; j < b->size; j++) {
            Value valB = b->elements[i];

            if (!valuesEqual(valA, valB)) {
                return false;
            }
        }
    }

    return true;
}

Value indexTuple(ObjTuple* tuple, int index) {
    index = validateIndex(index, tuple->size);
    return tuple->elements[index];
}

ObjTuple* sliceTuple(GC* gc, ObjTuple* tuple, int start, int end) {
    start = validateIndex(start, tuple->size);
    end = validateIndex(end, tuple->size);

    size_t length = (start <= end && start < tuple->size) ? end - start + 1 : 0;

    pushTemp(gc, OBJ_VAL(tuple));

    ObjTuple* result = ALLOCATE_OBJ(gc, ObjTuple, OBJ_TUPLE, true);

    Value* elements = ALLOCATE(gc, Value, length);
    memcpy(elements, tuple->elements + start, length * sizeof(Value));

    result->size = length;
    result->elements = elements;

    popTemp(gc);

    return result;
}

/**
 * @brief Concatenate tuples a and b if both are tuples.
 * 
 * @param a A tuple
 * @param b A tuple
 * @return  The concatenated tuple
 */
ObjTuple* concatenateTuple(GC* gc, ObjTuple* a, ObjTuple* b) {
    pushTemp(gc, OBJ_VAL(a));
    pushTemp(gc, OBJ_VAL(b));

    ObjTuple* tuple = ALLOCATE_OBJ(gc, ObjTuple, OBJ_TUPLE, true);
    pushTemp(gc, OBJ_VAL(tuple));
    tuple->size = 0;
    tuple->elements = NULL;
    
    size_t size = a->size + b->size;
    Value* elements = ALLOCATE(gc, Value, size);
    memcpy(elements, a->elements, a->size * sizeof(Value));
    memcpy(elements + a->size, b->elements, b->size * sizeof(Value));

    tuple->elements = elements;
    tuple->size = size;

    popTemp(gc);
    popTemp(gc);
    popTemp(gc);
    
    return tuple;
}

void printTuple(ObjTuple* tuple) {
    int numElements = tuple->size;
    int count = 0;
    
    fputc('{', stdout);
    for (int i = 0; i < tuple->size; i++) {
        Value value = tuple->elements[i];

        if (IS_OBJ(value) && IS_STRING(value)) {
            fputc('"', stdout);
            printValue(value, false);
            fputc('"', stdout);
        } else if (IS_CHAR(value)) {
            fputc('\'', stdout);
            printValue(value, false);
            fputc('\'', stdout);
        } else {
            printValue(value, false);
        }

        if (count < numElements - 1) fputs(", ", stdout);
        count++;
    }
    fputc('}', stdout);
}   

/**
 * @brief Converts a tuple object to a string.
 * 
 * @param tuple Pointer to the tuple
 * @return      A list of characters
 * 
 * Must be freed.
 */
unsigned char* tupleToString(ObjTuple* tuple) {
    // Create an empty string builder
    StringBuilder* sb = sb_create();
    char* str = NULL;

    sb_append(sb, "(");
    
    // Append elements
    int numElements = tuple->size;
    for (int i = 0; i < numElements; i++) {
        Value value = tuple->elements[i];
        
        unsigned char* str = valueToString(value);
        if (IS_OBJ(value) && IS_STRING(value)) {
            sb_appendf(sb, "\"%s\"", str);
        } else if (IS_CHAR(value)) {
            sb_appendf(sb, "'%s'", str);
        } else {
            sb_appendf(sb, "%s", str);
        }
        free(str);

        if (i < numElements - 1) sb_append(sb, ", ");
    }

    sb_append(sb, ")");
    str = sb_concat(sb);

    // Clean up
    sb_free(sb);

    return str;
}