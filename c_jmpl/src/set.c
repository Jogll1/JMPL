#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "set.h"
#include "memory.h"
#include "object.h"
#include "debug.h"
#include "gc.h"
#include "hash.h"
#include "../lib/c-stringbuilder/sb.h"
#include "../lib/stricks/stx.h"

#define SET_MAX_LOAD 0.65
#define MAX_PRINT_ELEMENTS 100

static void initSet(ObjSet* set) {
    set->count = 0;
    set->capacity = 0;
    set->entries = NULL;
}

static void printDebugSet(ObjSet* set) {
    printf("\n==============\n");
    for (int i = 0; i < set->capacity; i++) {
        unsigned char* str = valueToString(getSetValue(set, i));
        printf("%d. %s\n", i, str);
        free(str);
    }
    printf("==============\n");
}

static SetEntry* findEntry(SetEntry* entries, size_t capacity, Value key, hash_t hash) {
    // Map the key's hash code to an index in the array
    size_t index = hash & (capacity - 1);
    uint64_t perturb = hash;
    
    while (true) {
        SetEntry* entry = &entries[index];

        if (IS_NULL(entry->key) || valuesEqual(entry->key, key)) {
            return entry;
        }

        // Collision, so start probing
        index = (index * 5 + 1 + perturb) & (capacity - 1);
        perturb >>= 5;
    }
}

static void adjustCapacity(GC* gc, ObjSet* set, size_t capacity) {
    SetEntry* entries = ALLOCATE(gc, SetEntry, capacity);
    // Initialise every entry's key to be null
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL_VAL;
    }

    // Insert entries into array
    set->count = 0;
    for (int i = 0; i < set->capacity; i++) {
        SetEntry* entry = &set->entries[i];
        if(IS_NULL(entry->key)) continue;

        SetEntry* destination = findEntry(entries, capacity, entry->key, entry->hash);
        destination->key = entry->key;
        destination->hash = entry->hash;
        set->count++;
    }

    FREE_ARRAY(gc, SetEntry, set->entries, set->capacity);
    set->entries = entries;
    set->capacity = capacity;
}

// --- Sets --- 
ObjSet* newSet(GC* gc) {
    ObjSet* set = ALLOCATE_OBJ(gc, ObjSet, OBJ_SET, true);
    initSet(set);
    return set;
}

void freeSet(GC* gc, ObjSet* set) {
    FREE_ARRAY(gc, SetEntry, set->entries, set->capacity);
    initSet(set);
    FREE(gc, ObjSet, set); 
}

bool setInsert(GC* gc, ObjSet* set, Value value) {
    pushTemp(gc, OBJ_VAL(set));

    if(set->count + 1 > set->capacity * SET_MAX_LOAD) {
        size_t capacity = GROW_CAPACITY(set->capacity);
        adjustCapacity(gc, set, capacity);
    }

    hash_t hash = hashValue(value);

    SetEntry* entry = findEntry(set->entries, set->capacity, value, hash);
    bool isNewKey = IS_NULL(entry->key); // Change for better NULL entry checking
    if (isNewKey) set->count++;

    entry->key = value;
    entry->hash = hash;

    popTemp(gc);
    return isNewKey;
}

bool setContains(ObjSet* set, Value value) {
    if(set->count == 0) return false;

    SetEntry* entry = findEntry(set->entries, set->capacity, value, hashValue(value));

    return !IS_NULL(entry->key);
}

bool setsEqual(ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    if (a->count != b->count) return false;
    
    for (int i = 0; i < a->capacity; i++) {
        Value valA = getSetValue(a, i);
        if (IS_NULL(valA)) continue;

        if (!setContains(b, valA)) {
            return false;
        }
    }

    return true;
}

ObjSet* setIntersect(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    ObjSet* result = newSet(gc);
    pushTemp(gc, OBJ_VAL(result));

    // Iterate through smaller set
    if (a->count > b->count) {
        ObjSet* temp = a;
        a = b;
        b = temp;
    }

    for (int i = 0; i < a->capacity; i++) {
        Value valA = getSetValue(a, i);
        if (IS_NULL(valA)) continue;

        if (setContains(b, valA)) setInsert(gc, result, valA);
    }

    popTemp(gc);
    return result;
}

ObjSet* setUnion(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    ObjSet* result = newSet(gc);
    pushTemp(gc, OBJ_VAL(result));

    // Duplicate larger set
    if (a->count < b->count) {
        ObjSet* temp = a;
        a = b;
        b = temp;
    }

    // Mem copy a
    SetEntry* entries = ALLOCATE(gc, SetEntry, a->capacity);
    memcpy(entries, a->entries, a->capacity * sizeof(SetEntry));

    result->entries = entries;
    result->capacity = a->capacity;
    result->count = a->count;

    // Add in b - DO NOT COPY B - THE HASHES WILL BE MESSED UP
    for (int i = 0; i < b->capacity; i++) {
        Value valB = getSetValue(b, i);
        if (IS_NULL(valB)) continue;
        setInsert(gc, result, valB);
    }

    popTemp(gc);
    return result;
}

ObjSet* setDifference(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    ObjSet* result = newSet(gc);
    pushTemp(gc, OBJ_VAL(result));

    for (int i = 0; i < a->capacity; i++) {
        Value valA = getSetValue(a, i);
        if (IS_NULL(valA)) continue;

        if (!setContains(b, valA)) {
            setInsert(gc, result, valA);
        }
    }

    popTemp(gc);
    return result;
}

bool isSubset(ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);
    
    if (a->count > b->count) return false;

    for (int i = 0; i < a->capacity; i++) {
        Value valA = getSetValue(a, i);
        if (IS_NULL(valA)) continue;
        
        if (!setContains(b, valA)) {
            return false;
        }
    }

    return true;
}

bool isProperSubset(ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);
    if (a->count == b->count) return false;

    return isSubset(a, b);
}

Value getArb(ObjSet* set) {
    if (set->capacity == 0) return NULL_VAL;

    int randIndex = rand() % set->capacity;

    for (int i = randIndex; i < set->capacity; i++) {
        Value val = getSetValue(set, i);
        if (IS_NULL(val)) continue;

        return val;
    }
    
    return getArb(set); // Repeat until one is found
}

void printSet(ObjSet* set) {
    int numElements = set->count;
    int count = 0;
    
    fputc('{', stdout);
    for (int i = 0; i < set->capacity; i++) {
        Value value = getSetValue(set, i);
        if (!IS_NULL(value)) {
            if (IS_OBJ(value) && IS_STRING(value)) {
                fputc('"', stdout);
                printValue(getSetValue(set, i), false);
                fputc('"', stdout);
            } else if (IS_CHAR(value)) {
                fputc('\'', stdout);
                printValue(getSetValue(set, i), false);
                fputc('\'', stdout);
            } else {
                printValue(getSetValue(set, i), false);
            }

            if (count < numElements - 1) fputs(", ", stdout);
            count++;
        }
    }
    fputc('}', stdout);
}   

/**
 * @brief Converts a set to a C string.
 * 
 * @param set Pointer to the set
 * @return    An array of characters
 * 
 * Must be freed.
 */
unsigned char* setToString(ObjSet* set) {
    // // Create an empty string builder
    // StringBuilder* sb = sb_create();
    // char* str = NULL;

    // sb_append(sb, "{");
    
    // // Append elements
    // int numElements = set->count;
    // int count = 0;

    // for (int i = 0; i < set->capacity; i++) {
    //     Value value = getSetValue(set, i);
    //     if (!IS_NULL(value)) {
    //         // if (count == MAX_PRINT_ELEMENTS) {
    //         //     sb_appendf(sb, "...");
    //         //     break;
    //         // }
            
    //         unsigned char* str = valueToString(value);
    //         if (IS_OBJ(value) && IS_STRING(value)) {
    //             sb_appendf(sb, "\"%s\"", str);
    //         } else if (IS_CHAR(value)) {
    //             sb_appendf(sb, "'%s'", str);
    //         } else {
    //             sb_appendf(sb, "%s", str);
    //         }
    //         free(str);

    //         if (count < numElements - 1) sb_append(sb, ", ");
    //         count++;
    //     }
    // }

    // sb_append(sb, "}");
    // str = sb_concat(sb);

    // // Clean up
    // sb_free(sb);

    // return str;


    // Create an empty string builder
    stx_t stxStr = stx_from("{");

    // Append elements
    int numElements = set->count;
    int count = 0;

    for (int i = 0; i < set->capacity; i++) {
        Value value = getSetValue(set, i);
        if (!IS_NULL(value)) {
            unsigned char* str = valueToString(value);
            if (IS_OBJ(value) && IS_STRING(value)) {
                stx_append_fmt(&stxStr, "\"%s\"", str);
            } else if (IS_CHAR(value)) {
                stx_append_fmt(&stxStr, "'%s'", str);
            } else {
                stx_append_fmt(&stxStr, "%s", str);
            }
            free(str);

            if (count < numElements - 1) stx_append(&stxStr, ", ", 2);
            count++;
        }
    }

    return (unsigned char*)stxStr;
}