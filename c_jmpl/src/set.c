#include <stdlib.h>
#include <stdio.h>

#include "set.h"
#include "memory.h"
#include "object.h"
#include "debug.h"
#include "../lib/c-stringbuilder/sb.h"

// Max elements to convert to string in a set
#define MAX_STRING_SET_ELEMS 10 

// --- Sets --- 
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

ObjSet* setIntersect(ObjSet* a, ObjSet* b) {
    ObjSet* result = newSet();

    // Iterate through smaller set
    if (a->elements.count > b->elements.count) {
        ObjSet* temp = a;
        a = b;
        b = temp;
    }

    for (int i = 0; i < a->elements.capacity; i++) {
        Value valA = a->elements.entries[i].key;
        if (IS_NULL(valA)) continue;

        if (setContains(b, valA)) setInsert(result, valA);
    }

    return result;
}

ObjSet* setUnion(ObjSet* a, ObjSet* b) {
    ObjSet* result = newSet();

    for (int i = 0; i < a->elements.capacity; i++) {
        Value valA = a->elements.entries[i].key;
        if (IS_NULL(valA)) continue;

        setInsert(result, valA);
    }

    for (int i = 0; i < b->elements.capacity; i++) {
        Value valB = b->elements.entries[i].key;
        if (IS_NULL(valB)) continue;

        setInsert(result, valB);
    }

    return result;
}

ObjSet* setDifference(ObjSet* a, ObjSet* b) {
    ObjSet* result = newSet();

    for (int i = 0; i < a->elements.capacity; i++) {
        Value valA = a->elements.entries[i].key;
        if (IS_NULL(valA)) continue;

        if (!setContains(b, valA)) {
            setInsert(result, valA);
        }
    }

    return result;
}

bool isProperSubset(ObjSet* a, ObjSet* b) {
    if (a->elements.count >= b->elements.count) return false;

    for (int i = 0; i < a->elements.capacity; i++) {
        Value valA = a->elements.entries[i].key;
        if (IS_NULL(valA)) continue;
        
        if (!setContains(b, valA)) {
            return false;
        }
    }

    return true;
}

bool isSubset(ObjSet* a, ObjSet* b) {
    if (a->elements.count > b->elements.count) return false;

    for (int i = 0; i < a->elements.capacity; i++) {
        Value valA = a->elements.entries[i].key;
        if (IS_NULL(valA)) continue;
        
        if (!setContains(b, valA)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Hashes a set using the FNV-1a hashing algorithm.
 * 
 * @param set The set to hash
 * @return    A hashed form of the set
 */
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
        Value value = set->elements.entries[i].key;
        if (!IS_NULL(value)) {
            if (IS_OBJ(value) && IS_STRING(value)) {
                sb_appendf(sb, "\"%s\"", valueToString(value)->chars);
            } else {
                sb_appendf(sb, "%s", valueToString(value)->chars);
            }

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

// --- Set Iterator --- 
ObjSetIterator* newSetIterator(ObjSet* set) {
    ObjSetIterator* iterator = (ObjSetIterator*)allocateObject(sizeof(ObjSetIterator), OBJ_SET_ITERATOR);
    iterator->set = set;
    iterator->currentIndex = -1;

    // Find index of first value in set
    for (int i = 0; i < set->elements.capacity; i++) {
        if (IS_NULL(set->elements.entries[i].key)) continue;

        iterator->currentIndex = i;
        break;
    }

    return iterator;
}

bool freeSetIterator(ObjSetIterator* iterator) {
    FREE(ObjSetIterator, iterator);
}

/**
 * @brief Iterates the iterator if possible
 * 
 * @param iterator A pointer to an iterator object
 * @param value    A pointer to the current value 
 * @return         If the current iterator value is valid
 */
bool iterateSetIterator(ObjSetIterator* iterator, Value* value) {
    ObjSet* set = iterator->set;

    int capacity = set->elements.capacity;
    int current = iterator->currentIndex;
    if (current == -1 || capacity == 0 || current >= capacity) return false;

    // Set value to point to the value pre-iteration
    *(value) = set->elements.entries[current].key;

    for (int i = current + 1; i < capacity; i++) {
        if (IS_NULL(set->elements.entries[i].key)) continue;

        iterator->currentIndex = i;
        return true;
    }

    iterator->currentIndex = capacity;
    return true; 
}
