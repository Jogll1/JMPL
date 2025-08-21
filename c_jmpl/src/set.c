#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "set.h"
#include "memory.h"
#include "object.h"
#include "debug.h"
#include "gc.h"
#include "hash.h"
#include "../lib/c-stringbuilder/sb.h"

#define SET_MAX_LOAD 0.75
#define MAX_PRINT_ELEMENTS 100

#pragma region Finite Set 
// ======================================================================
// =================             Finite Set             =================
// ======================================================================

static void initFiniteSet(FiniteSet* set) {
    set->count = 0;
    set->capacity = 0;
    set->elements = NULL;
}

FiniteSet* newFiniteSet(GC* gc) {
    FiniteSet* set = AS_FINITE_SET(ALLOCATE_OBJ(gc, ObjSet, OBJ_SET, true));
    initFiniteSet(set);

    return set;
}

static Value* findElement(Value* elements, int capacity, Value value) {
    // Map the key's hash code to an index in the array
    hash_t index = hashValue(value) & (capacity - 1);
    
    while (true) {
        Value* element = &elements[index];

        if (IS_NULL(*element) || valuesEqual(*element, value)) {
            return element;
        }

        // Collision, so start linear probing
        index = (index + 1) & (capacity - 1);
    }
}

static void adjustCapacity(GC* gc, FiniteSet* set, int capacity) {
    Value* elements = ALLOCATE(gc, Value, capacity);
    // Initialise every element to be null
    for (int i = 0; i < capacity; i++) {
        elements[i] = NULL_VAL;
    }

    // Insert entries into array
    set->count = 0;
    for (int i = 0; i < set->capacity; i++) {
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

static void freeFiniteSet(GC* gc, FiniteSet* set) {
    FREE_ARRAY(gc, Value, set->elements, set->capacity);
    initFiniteSet(set);
}

static bool finiteSetInsert(GC* gc, FiniteSet* set, Value value) {
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

static bool finiteSetContains(FiniteSet* set, Value value) {
    if(set->count == 0) return false;

    Value* element = findElement(set->elements, set->capacity, value);

    return !IS_NULL(*element);
}

static bool finiteSetsEqual(FiniteSet* a, FiniteSet* b) {
    if (a->count != b->count) return false;
    
    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;

        if (!finiteSetContains(b, valA)) {
            return false;
        }
    }

    return true;
}

static FiniteSet* finiteSetsIntersect(GC* gc, FiniteSet* a, FiniteSet* b) {
    FiniteSet* result = newFiniteSet(gc);
    pushTemp(gc, OBJ_VAL(result));

    // Iterate through smaller set
    if (a->count > b->count) {
        FiniteSet* temp = a;
        a = b;
        b = temp;
    }

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;

        if (finiteSetContains(b, valA)) finiteSetInsert(gc, result, valA);
    }

    popTemp(gc);
    return result;
}

static FiniteSet* finiteSetsUnion(GC* gc, FiniteSet* a, FiniteSet* b) {
    FiniteSet* result = newFiniteSet(gc);
    pushTemp(gc, OBJ_VAL(result));

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;
        finiteSetInsert(gc, result, valA);
    }

    for (int i = 0; i < b->capacity; i++) {
        Value valB = b->elements[i];
        if (IS_NULL(valB)) continue;
        finiteSetInsert(gc, result, valB);
    }

    popTemp(gc);
    return result;
}

static FiniteSet* finiteSetsDifference(GC* gc, FiniteSet* a, FiniteSet* b) {
    FiniteSet* result = newFiniteSet(gc);
    pushTemp(gc, OBJ_VAL(result));

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;

        if (!finiteSetContains(b, valA)) {
            finiteSetInsert(gc, result, valA);
        }
    }

    popTemp(gc);
    return result;
}

static bool isFiniteSubset(FiniteSet* a, FiniteSet* b) {
    if (a->count > b->count) return false;

    for (int i = 0; i < a->capacity; i++) {
        Value valA = a->elements[i];
        if (IS_NULL(valA)) continue;
        
        if (!finiteSetContains(b, valA)) {
            return false;
        }
    }

    return true;
}

static bool isProperFiniteSubset(FiniteSet* a, FiniteSet* b) {
    if (a->count == b->count) return false;

    return isFiniteSubset(a, b);
}

static Value finiteGetArb(FiniteSet* set) {
    if (set->capacity == 0) return NULL_VAL;

    int randIndex = rand() % set->capacity;

    for (int i = randIndex; i < set->capacity; i++) {
        Value val = set->elements[i];
        if (IS_NULL(val)) continue;

        return val;
    }
    
    return finiteGetArb(set); // Repeat until one is found
}

static unsigned char* finiteSetToString(FiniteSet* set) {
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

#pragma endregion

#pragma region Range Set 
// ======================================================================
// =================             Range Set              =================
// ======================================================================

#define RANGE_SET_LOOP

RangeSet* newRangeSet(GC* gc, int start, int end, int step) {
    assert(step != 0);

    RangeSet* set = AS_RANGE_SET(ALLOCATE_OBJ(gc, ObjSet, OBJ_SET, true));
    
    set->start = start;
    set->end = end;
    set->step = step;

    // Size
    int size = abs(set->start - set->end);
    if (set->start > 1) {
        size = (int)floorl((double)size / (double)(set->step));
    }
    size++;

    set->size = size;

    return set;
}

static FiniteSet* rangeToFiniteSet(GC* gc, RangeSet* set) {
    FiniteSet* finiteSet = newFiniteSet(gc);

    int current = set->start;
    int step = set->start < set->end ? set->step : -set->step;
    for (int i = 0; i < set->size; i++) {
        finiteSetInsert(gc, finiteSet, NUMBER_VAL(current));
        current += step;
    }
    
    return finiteSet;
}

static bool rangeSetsEqual(RangeSet* a, RangeSet* b) {
    return (a->start == b->start) &&
           (a->end == b->end) &&
           (a->step == b->step);
}

static bool rangeSetContains(RangeSet* set, Value value) {
    if (!IS_INTEGER(value)) {
        return false;
    }

    int val = (int)AS_NUMBER(value);

    if (val < set->start || val > set->end) {
        return false;
    }

    if (set->step == 1) {
        return true;
    }

    return (val - set->start) % set->step == 0;
}

static Value rangeGetArb(RangeSet* set) {
    if (set->size == 0) return NULL_VAL;

    return set->start + rand() % set->size;
}

static unsigned char* rangeSetToString(RangeSet* set) {
    // Create an empty string builder
    StringBuilder* sb = sb_create();
    char* str = NULL;

    sb_append(sb, "{");
    
    int current = set->start;
    int step = set->start < set->end ? set->step : -set->step;
    for (int i = 0; i < set->size; i++) {
        sb_appendf(sb, "%d", current);
        if (i < set->size - 1) sb_append(sb, ", ");
        
        current += step;
    }

    sb_append(sb, "}");
    str = sb_concat(sb);

    // Clean up
    sb_free(sb);

    return str;
}

#pragma endregion

#pragma region Set 
// ======================================================================
// =================                Set                 =================
// ======================================================================

void freeSet(GC* gc, ObjSet* set) {
    if (set->type == SET_FINITE) {
        freeFiniteSet(gc, AS_FINITE_SET(set));
    }

    FREE(gc, ObjSet, set); 
}

void markSet(GC* gc, ObjSet* set) {
    switch (set->type) {
        case SET_FINITE: {
            FiniteSet* set = AS_FINITE_SET(set);
            for (int i = 0; i < set->capacity; i++) {
                markValue(gc, set->elements[i]);
            }
            break;
        }
    }
}

size_t getSetSize(ObjSet* set) {
    switch (set->type) {
        case SET_FINITE: return AS_FINITE_SET(set)->count;
        case SET_RANGE:  return AS_RANGE_SET(set)->size;
    }
}

bool setInsert(GC* gc, ObjSet* set, Value value) {
    bool isNewKey = false;

    switch (set->type) {
        case SET_FINITE: isNewKey = finiteSetInsert(gc, AS_FINITE_SET(set), value); break;
    }

    return isNewKey;
}

bool setContains(ObjSet* set, Value value) {
    switch (set->type) {
        case SET_FINITE: return finiteSetContains(AS_FINITE_SET(set), value);
        case SET_RANGE:  return rangeSetContains(AS_RANGE_SET(set), value);
    }
}

bool setsEqual(ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    if (getSetSize(a) != getSetSize(b)) return false;

    bool fA = IS_FINITE_SET(a);
    bool fB = IS_FINITE_SET(b);

    if (fA && fB) {
        return finiteSetsEqual(AS_FINITE_SET(a), AS_FINITE_SET(b));
    } else if (!fA && !fB) {
        return rangeSetsEqual(AS_RANGE_SET(a), AS_RANGE_SET(b));
    } else {
        RangeSet* r = fA ? AS_RANGE_SET(b) : AS_RANGE_SET(a);
        FiniteSet* f = fA ? AS_FINITE_SET(a) : AS_FINITE_SET(b);

        int current = r->start;
        int step = r->start < r->end ? r->step : -r->step;
        for (int i = 0; i < r->size; i++) {
            if (!finiteSetContains(f, NUMBER_VAL(i))) {
                return false;
            }
            current += step;
        }
    }

    return true;
}

ObjSet* setIntersect(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    bool fA = IS_FINITE_SET(a);
    bool fB = IS_FINITE_SET(b);

    if (fA && fB) {
        return (ObjSet*)finiteSetsIntersect(gc, AS_FINITE_SET(a), AS_FINITE_SET(b));
    } else {
        FiniteSet* fSetA = fA ? AS_FINITE_SET(a) : rangeToFiniteSet(gc, AS_RANGE_SET(a));
        FiniteSet* fSetB = fB ? AS_FINITE_SET(b) : rangeToFiniteSet(gc, AS_RANGE_SET(b));

        return (ObjSet*)finiteSetsIntersect(gc, fSetA, fSetB);
    }
}

ObjSet* setUnion(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    bool fA = IS_FINITE_SET(a);
    bool fB = IS_FINITE_SET(b);

    if (fA && fB) {
        return (ObjSet*)finiteSetsUnion(gc, AS_FINITE_SET(a), AS_FINITE_SET(b));
    } else {
        FiniteSet* fSetA = fA ? AS_FINITE_SET(a) : rangeToFiniteSet(gc, AS_RANGE_SET(a));
        FiniteSet* fSetB = fB ? AS_FINITE_SET(b) : rangeToFiniteSet(gc, AS_RANGE_SET(b));

        return (ObjSet*)finiteSetsUnion(gc, fSetA, fSetB);
    }
}

ObjSet* setDifference(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    bool fA = IS_FINITE_SET(a);
    bool fB = IS_FINITE_SET(b);

    if (fA && fB) {
        return (ObjSet*)finiteSetsDifference(gc, AS_FINITE_SET(a), AS_FINITE_SET(b));
    } else {
        FiniteSet* fSetA = fA ? AS_FINITE_SET(a) : rangeToFiniteSet(gc, AS_RANGE_SET(a));
        FiniteSet* fSetB = fB ? AS_FINITE_SET(b) : rangeToFiniteSet(gc, AS_RANGE_SET(b));

        return (ObjSet*)finiteSetsDifference(gc, fSetA, fSetB);
    }
}

bool isSubset(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);
    
    bool fA = IS_FINITE_SET(a);
    bool fB = IS_FINITE_SET(b);

    if (fA && fB) {
        return (ObjSet*)isFiniteSubset(AS_FINITE_SET(a), AS_FINITE_SET(b));
    } else {
        FiniteSet* fSetA = fA ? AS_FINITE_SET(a) : rangeToFiniteSet(gc, AS_RANGE_SET(a));
        FiniteSet* fSetB = fB ? AS_FINITE_SET(b) : rangeToFiniteSet(gc, AS_RANGE_SET(b));

        return (ObjSet*)finiteSetsDifference(gc, fSetA, fSetB);
    }
}

bool isProperSubset(GC* gc, ObjSet* a, ObjSet* b) {
    assert(a != NULL && b != NULL);

    if (getSetSize(a) == getSetSize(b)) return false;

    return isSubset(gc, a, b);
}

Value getArb(ObjSet* set) {
    switch (set->type) {
        case SET_FINITE: return finiteGetArb(AS_FINITE_SET(set));
        case SET_RANGE:  return rangeGetArb(AS_RANGE_SET(set));
    }
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
    unsigned char* str;

    switch (set->type) {
        case SET_FINITE: str = finiteSetToString(AS_FINITE_SET(set)); break;
        case SET_RANGE:  str = rangeSetToString(AS_RANGE_SET(set)); break;
    }
    
    return str;
}

#pragma endregion