#include <stdlib.h>
#include <stdio.h>

#include "set.h"
#include "memory.h"
#include "object.h"

ObjSet* newSet() {
    ObjSet* set = (ObjSet*)allocateObject(sizeof(ObjSet), OBJ_SET);
    initValTable(&set->elements);
    return set;
}

bool setAdd(ObjSet* set, Value value) {
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

void printSet(ObjSet* set) {
    // For now print this
    printf("<set>"); 
}