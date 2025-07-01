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
    return valTableGet(&set->elements, value, &BOOL_VAL(true));
}

bool setRemove(ObjSet* set, Value value) {
    return valTableDelete(&set->elements, value);
}

bool freeSet(ObjSet* set) {
    freeValTable(&set->elements);
    FREE(ObjSet, set); 
}

unsigned char* setToString(ObjSet* set) {
    // Create an empty string builder
    StringBuilder* sb = sb_create();
    char* str = NULL;

    sb_append(sb, "{");
    
    // Append elements
    int numElements = set->elements.count;
    int count = 0;

    for (int i = 0; i < numElements; i++) {
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