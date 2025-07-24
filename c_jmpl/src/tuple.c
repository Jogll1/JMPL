#include <stdlib.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "tuple.h"
#include "../lib/c-stringbuilder/sb.h"

ObjTuple* newTuple(int size) {
    ObjTuple* tuple = (ObjTuple*)allocateObject(sizeof(ObjTuple), OBJ_TUPLE);
    tuple->size = size;
    tuple->elements = ALLOCATE(Value, size); 
    
    for (int i = 0; i < size; i++) {
        tuple->elements[i] = NULL_VAL;
    }
    
    return tuple;
}


bool tuplesEqual(ObjTuple* a, ObjTuple* b) {
    if (a->size != b->size) return false;
    
    for (int i = 0; i < a->size; i++) {
        Value valA = a->elements[i];

        for (int j = 0; j < b->size; j++) {
            Value valB = b->elements[i];

            if (!valuesEqual(valA, valB)) {
                return false;
            }
        }
    }

    return true;
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
                
        if (IS_OBJ(value) && IS_STRING(value)) {
            sb_appendf(sb, "\"%s\"", valueToString(value)->chars);
        } else {
            sb_appendf(sb, "%s", valueToString(value)->chars);
        }

        if (i < numElements - 1) sb_append(sb, ", ");
    }

    sb_append(sb, ")");
    str = sb_concat(sb);

    // Clean up
    sb_free(sb);

    return str;
}

/**
 * @brief Concatenate tuples a and b if both are tuples.
 * 
 * @param a A tuple
 * @param b A tuple
 * @return  The concatenated tuple
 */
ObjTuple* concatenateTuple(ObjTuple* a, ObjTuple* b) {
    ObjTuple* tuple = (ObjTuple*)allocateObject(sizeof(ObjTuple), OBJ_TUPLE);
    int size = a->size + b->size;
    tuple->size = size;
    tuple->elements = ALLOCATE(Value, size); 
    
    for (int i = 0; i < a->size; i++) {
        tuple->elements[i] = a->elements[i];
    }

    for (int i = 0; i < b->size; i++) {
        tuple->elements[i + a->size] = b->elements[i];
    }
    
    return tuple;
}

/**
 * @brief Hashes a tuple using the FNV-1a hashing algorithm.
 * 
 * @param tuple The tuple to hash
 * @return      A hashed form of the tuple
 */
uint32_t hashTuple(ObjTuple* tuple) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < tuple->size; i++) {
        Value value = tuple->elements[i];

        uint32_t elemHash = hashValue(value);

        hash ^= elemHash;
        hash *= 16777619;
    }
}