#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "set.h"
#include "memory.h"
#include "object.h"
#include "debug.h"
#include "gc.h"
#include "hash.h"
#include "../lib/c-stringbuilder/sb.h"

#define SET_MAX_LOAD 0.75
#define MAX_PRINT_ELEMENTS 100

static void initSet(ObjSet* set) {
    set->count = 0;
    set->capacity = 0;
    set->elements = NULL;
}

static void printDebugSet(GC* gc, ObjSet* set) {
    printf("\n==============\n");
    for (int i = 0; i < set->capacity; i++) {
        unsigned char* str = valueToString(set->elements[i]);
        printf("%d. %s\n", i, str);
        free(str);
    }
    printf("==============\n");
}

static Value* findElement(Value* elements, int capacity, Value value) {
    // Map the key's hash code to an index in the array
    uint32_t index = hashValue(value) & (capacity - 1);
    
    while (true) {
        Value* element = &elements[index];

        if (IS_NULL(*element) || valuesEqual(*element, value)) {
            return element;
        }

        // Collision, so start linear probing
        index = (index + 1) & (capacity - 1);
    }
}

static void adjustCapacity(GC* gc, ObjSet* set, int capacity) {
    Value* elements = ALLOCATE(gc, Value, capacity);
    // Initialise every element to be null
    for(int i = 0; i < capacity; i++) {
        elements[i] = NULL_VAL;
    }

    // Insert entries into array
    set->count = 0;
    for(int i = 0; i < set->capacity; i++) {
        Value element = set->elements[i];
        if(IS_NULL(element)) continue;

        Value* destination = findElement(elements, capacity, element);
        *destination = element;
        set->count++;
    }

    FREE_ARRAY(gc, Value, set->elements, set->capacity);
    set->elements = elements;
    set->capacity = capacity;
}

// --- Sets --- 
ObjSet* newSet(GC* gc) {
    ObjSet* set = ALLOCATE_OBJ(gc, ObjSet, OBJ_SET);
    initSet(set);
    return set;
}

void freeSet(GC* gc, ObjSet* set) {
    FREE_ARRAY(gc, Value, set->elements, set->capacity);
    initSet(set);
    FREE(gc, ObjSet, set); 
}

bool setInsert(GC* gc, ObjSet* set, Value value) {
    pushTemp(gc, OBJ_VAL(set));

    if(set->count + 1 > set->capacity * SET_MAX_LOAD) {
        int capacity = GROW_CAPACITY(set->capacity);
        adjustCapacity(gc, set, capacity);
    }

    Value* element = findElement(set->elements, set->capacity, value);
    bool isNewKey = IS_NULL(*element);
    if (isNewKey) set->count++;

    *element = value;

    popTemp(gc);
    return isNewKey;
}

bool setContains(ObjSet* set, Value value) {
    if(set->count == 0) return false;

    Value* element = findElement(set->elements, set->capacity, value);

    return !IS_NULL(*element);
}

bool setsEqual(ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    if (a->count != b->count) return false;
    
    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
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
        Value valA = a->elements[i];
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

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;
        setInsert(gc, result, valA);
    }

    for (int i = 0; i < b->capacity; i++) {
        Value valB = b->elements[i];
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
        Value valA = a->elements[i];
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
        Value valA = a->elements[i];
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
    for (int i = 0; i < set->capacity; i++) {
        Value val = set->elements[i];
        if (IS_NULL(val)) continue;

        return val;
    }
    
    return NULL_VAL;
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
            // if (count == MAX_PRINT_ELEMENTS) {
            //     sb_appendf(sb, "...");
            //     break;
            // }
            
            unsigned char* str = valueToString(value);
            if (IS_OBJ(value) && IS_STRING(value)) {
                sb_appendf(sb, "\"%s\"", str);
            } else if (IS_CHAR(value)) {
                sb_appendf(sb, "'%s'", str);
            } else {
                sb_appendf(sb, "%s", str);
            }
            free(str);

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
ObjSetIterator* newSetIterator(GC* gc, ObjSet* set) {
    ObjSetIterator* iterator = ALLOCATE_OBJ(gc, ObjSetIterator, OBJ_SET_ITERATOR);
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

void freeSetIterator(GC* gc, ObjSetIterator* iterator) {
    FREE(gc, ObjSetIterator, iterator);
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