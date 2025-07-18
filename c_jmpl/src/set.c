#include <stdlib.h>
#include <stdio.h>

#include "set.h"
#include "memory.h"
#include "object.h"
#include "debug.h"
#include "../lib/c-stringbuilder/sb.h"

#define SET_MAX_LOAD 0.75

static void initSet(ObjSet* set) {
    set->count = 0;
    set->capacity = 0;
    set->elements = NULL;
}

static void printDebugSet(ObjSet* set) {
    printf("\n==============\n");
    for (int i = 0; i < set->capacity; i++) {
        printf("%d. %s\n", i, valueToString(set->elements[i])->chars);
    }
    printf("==============\n");
}

static Value* findElement(Value* elements, int capacity, Value value) {
    // Map the key's hash code to an index in the array
    uint32_t index = hashValue(value) % capacity;

    while (true) {
        Value* element = &elements[index];

        if (IS_NULL(*element) || valuesEqual(*element, value)) {
            return element;
        }

        // Collision, so start linear probing
        index = (index + 1) % capacity;
    }
}

static void adjustCapacity(ObjSet* set, int capacity) {
    Value* elements = ALLOCATE(Value, capacity);
    // Initialise every element to be null
    for(int i = 0; i < capacity; i++) {
        elements[i] = NULL_VAL;
    }

    // Insert entries into array
    set->count = 0;
    for(int i = 0; i < set->capacity; i++) {
        Value element = set->elements[i];
        if(element.type == VAL_NULL) continue;

        Value* destination = findElement(elements, capacity, element);
        *destination = element;
        set->count++;
    }

    FREE_ARRAY(Value, set->elements, set->capacity);
    set->elements = elements;
    set->capacity = capacity;
}

// --- Sets --- 
ObjSet* newSet() {
    ObjSet* set = (ObjSet*)allocateObject(sizeof(ObjSet), OBJ_SET);
    initSet(set);
    return set;
}

void freeSet(ObjSet* set) {
    FREE_ARRAY(Value, set->elements, set->capacity);
    initSet(set);
    FREE(ObjSet, set); 
}

bool setInsert(ObjSet* set, Value value) {
    if(set->count + 1 > set->capacity * SET_MAX_LOAD) {
        int capacity = GROW_CAPACITY(set->capacity);
        adjustCapacity(set, capacity);
    }

    Value* element = findElement(set->elements, set->capacity, value);
    bool isNewKey = IS_NULL(*element);
    if (isNewKey) set->count++;

    *element = value;

    return isNewKey;
}

bool setContains(ObjSet* set, Value value) {
    if(set->count == 0) return false;

    Value* element = findElement(set->elements, set->capacity, value);

    return !IS_NULL(*element);
}

bool setsEqual(ObjSet* a, ObjSet* b) {
    if (a->count != b->count) return false;
    
    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;
        bool found = false;

        for (int j = 0; j < a->capacity; j++) {
            Value valB = b->elements[j];
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

ObjSet* setIntersect(ObjSet* a, ObjSet* b) {
    ObjSet* result = newSet();

    // Iterate through smaller set
    if (a->count > b->count) {
        ObjSet* temp = a;
        a = b;
        b = temp;
    }

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;

        if (setContains(b, valA)) setInsert(result, valA);
    }

    return result;
}

ObjSet* setUnion(ObjSet* a, ObjSet* b) {
    ObjSet* result = newSet();

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;

        setInsert(result, valA);
    }

    for (int i = 0; i < b->capacity; i++) {
        Value valB = b->elements[i];
        if (IS_NULL(valB)) continue;

        setInsert(result, valB);
    }

    return result;
}

ObjSet* setDifference(ObjSet* a, ObjSet* b) {
    ObjSet* result = newSet();

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;

        if (!setContains(b, valA)) {
            setInsert(result, valA);
        }
    }

    return result;
}

bool isSubset(ObjSet* a, ObjSet* b) {
    if (a->count > b->count) return false;

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;
        
        if (!setContains(b, valA)) {
            return false;
        }
    }

    return true;
}

bool isProperSubset(ObjSet* a, ObjSet* b) {
    if (a->count == b->count) return false;

    return isSubset(a, b);
}

/**
 * @brief Hashes a set using the FNV-1a hashing algorithm.
 * 
 * @param set The set to hash
 * @return    A hashed form of the set
 */
uint32_t hashSet(ObjSet* set) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < set->capacity; i++) {
        Value element = set->elements[i];
        if (!IS_NULL(element)) {
            uint32_t elemHash = hashValue(element);

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
    int numElements = set->count;
    int count = 0;

    for (int i = 0; i < set->capacity; i++) {
        Value value = set->elements[i];
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
    for (int i = 0; i < set->capacity; i++) {
        if (IS_NULL(set->elements[i])) continue;

        iterator->currentIndex = i;
        break;
    }

    return iterator;
}

void freeSetIterator(ObjSetIterator* iterator) {
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

    int capacity = set->capacity;
    int current = iterator->currentIndex;
    if (current == -1 || capacity == 0 || current >= capacity) return false;

    // Set value to point to the value pre-iteration
    *(value) = set->elements[current];

    for (int i = current + 1; i < capacity; i++) {
        if (IS_NULL(set->elements[i])) continue;

        iterator->currentIndex = i;
        return true;
    }

    iterator->currentIndex = capacity;
    return true; 
}