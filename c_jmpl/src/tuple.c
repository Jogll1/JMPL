#include <stdlib.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "tuple.h"
#include "../lib/c-stringbuilder/sb.h"

ObjTuple* newTuple(int size) {
    ObjTuple* tuple = (ObjTuple*)allocateObject(sizeof(ObjTuple), OBJ_TUPLE);
    tuple->arity = size;
    tuple->elements = ALLOCATE(Value, size); 
    
    for (int i = 0; i < size; i++) {
        tuple->elements[i] = NULL_VAL;
    }
    
    return tuple;
}

unsigned char* tupleToString(ObjTuple* tuple) {
    // Create an empty string builder
    StringBuilder* sb = sb_create();
    char* str = NULL;

    sb_append(sb, "(");
    
    // Append elements
    int numElements = tuple->arity;
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

bool tuplesEqual(ObjTuple* a, ObjTuple* b) {
    if (a->arity != b->arity) return false;
    
    for (int i = 0; i < a->arity; i++) {
        Value valA = a->elements[i];

        for (int j = 0; j < b->arity; j++) {
            Value valB = b->elements[i];

            if (!valuesEqual(valA, valB)) {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Hashes a tuple using the FNV-1a hashing algorithm.
 * 
 * @param tuple The tuple to hash
 * @return      A hashed form of the tuple
 */
uint32_t hashTuple(ObjTuple* tuple) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < tuple->arity; i++) {
        Value value = tuple->elements[i];

        uint32_t elemHash = hashValue(value);

        hash ^= elemHash;
        hash *= 16777619;
    }
}