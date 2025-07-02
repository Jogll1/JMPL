#include <stdlib.h>
#include <stdio.h>

#include "set.h"
#include "memory.h"
#include "object.h"
#include "../lib/c-stringbuilder/sb.h"

// Max elements to convert to string in a set
#define MAX_STRING_SET_ELEMS 10 

ObjSet* newSet() {
    ObjSet* set = (ObjSet*)allocateObject(sizeof(ObjSet), OBJ_SET);
    initValTable(&set->elements);
    return set;
}

bool setInsert(ObjSet* set, Value value) {
    return valTableSet(&set->elements, value, BOOL_VAL(true));
}

bool setContains(ObjSet* set, Value value) {
    for (int i = 0; i < set->elements.capacity; i++) {
        Value val = set->elements.entries[i].key;
        if (IS_NULL(val)) continue;
    }

    return valTableGet(&set->elements, value, &BOOL_VAL(true));
}

bool setRemove(ObjSet* set, Value value) {
    return valTableDelete(&set->elements, value);
}

bool setsEqual(ObjSet* a, ObjSet* b) {
    if (a->elements.count != b->elements.count) return false;
    
    for (int i = 0; i < a->elements.capacity; i++) {
        Value valA = a->elements.entries[i].key;
        if (IS_NULL(valA)) continue;
        bool found = false;

        for (int j = 0; j < a->elements.capacity; j++) {
            Value valB = b->elements.entries[j].key;
            if (IS_NULL(valB)) continue;

            if (valuesEqual(valA, valB)) {
                found = true;
                break;
            }
        }

        if (!found) return false;
    }

    return true;
}

bool freeSet(ObjSet* set) {
    freeValTable(&set->elements);
    FREE(ObjSet, set); 
}

uint32_t hashSet(ObjSet* set) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < set->elements.capacity; i++) {
        ValEntry* entry = &set->elements.entries[i];
        if (!IS_NULL(entry->key)) {
            uint32_t elemHash = hashValue(entry->key);

            hash ^= elemHash;
            hash *= 16777619;
        }
    }
}

unsigned char* setToString(ObjSet* set) {
    // Create an empty string builder
    StringBuilder* sb = sb_create();
    char* str = NULL;

    sb_append(sb, "{");
    
    // Append elements
    int numElements = set->elements.count;
    int count = 0;

    for (int i = 0; i < set->elements.capacity; i++) {
        ValEntry* entry = &set->elements.entries[i];
        if (entry->key.type != VAL_NULL && !IS_NULL(entry->key)) {
            sb_appendf(sb, "%s", valueToString(entry->key)->chars);
            if (count < numElements - 1) sb_append(sb, ", ");
            count++;
        }
    }

    sb_append(sb, "}");

    str = sb_concat(sb);

    // Clean up
    sb_free(sb);

    return str;
}