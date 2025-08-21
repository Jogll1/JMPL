#include <assert.h>
#include <stdio.h>

#include "iterator.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
#include "obj_string.h"
#include "memory.h"

ObjIterator* newIterator(GC* gc, Obj* target) {
    assert(target->isIterable);

    ObjIterator* iterator = ALLOCATE_OBJ(gc, ObjIterator, OBJ_ITERATOR, false);
    iterator->target = target;
    iterator->currentIndex = -1;

    switch (target->type) {
        case OBJ_SET: {
            ObjSet* set = (ObjSet*)target;

            switch(set->type) {
                case SET_FINITE: {
                    FiniteSet* set = (FiniteSet*)set;
                    // Find index of first value in set
                    for (int i = 0; i < set->capacity; i++) {
                        if (IS_NULL(set->elements[i])) continue;

                        iterator->currentIndex = i;
                        break;
                    }
                    break;
                }
                case SET_RANGE: {
                    iterator->currentIndex = AS_RANGE_SET(set)->start;
                    break;
                }
            }
        }
        case OBJ_TUPLE: {
            ObjTuple* tuple = (ObjTuple*)target;

            if (tuple->size > 0) {
                iterator->currentIndex = 0;
            } 
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)target;

            if (string->length > 0) {
                iterator->currentIndex = 0;
            } 
            break;
        }
    }

    return iterator;
}

void freeIterator(GC* gc, ObjIterator* iterator) {
    FREE(gc, ObjIterator, iterator);
}

static bool iterateFiniteSet(ObjIterator* iterator, FiniteSet* set, Value* value) {
    int capacity = set->capacity;
    int current = iterator->currentIndex;
    if (current == -1 || capacity == 0 || current >= capacity) return false;

    // Set value to point to the value pre-iteration
    *(value) = set->elements[current];

    // Get next iteration
    for (int i = current + 1; i < capacity; i++) {
        if (IS_NULL(set->elements[i])) continue;

        iterator->currentIndex = i;
        return true;
    }

    iterator->currentIndex = capacity;
    return true;
}

static bool iterateRangeSet(ObjIterator* iterator, RangeSet* set, Value* value) {
    int capacity = set->size;
    int current = iterator->currentIndex;
    if (current == -1 || capacity == 0 || current >= capacity) return false;

    // Set value to point to the value pre-iteration
    *(value) = NUMBER_VAL(iterator->currentIndex);

    // Get next iteration
    iterator->currentIndex += set->step;
    return true;
}

static bool iterateSet(ObjIterator* iterator, Value* value) {
    assert(iterator->target->type == OBJ_SET);

    ObjSet* set = (ObjSet*)iterator->target;
    switch (set->type) {
        case SET_FINITE: return iterateFiniteSet(iterator, AS_FINITE_SET(set), value);
        case SET_RANGE:  return iterateRangeSet(iterator, AS_RANGE_SET(set), value);
    }

 
}

static bool iterateTuple(ObjIterator* iterator, Value* value) {
    assert(iterator->target->type == OBJ_TUPLE);

    ObjTuple* tuple = (ObjTuple*)iterator->target;

    int capacity = tuple->size;
    int current = iterator->currentIndex;
    if (current == -1 || capacity == 0 || current >= capacity) return false;

    // Set value to point to the value pre-iteration
    *(value) = tuple->elements[current];

    // Get next iteration
    iterator->currentIndex++;
    return true;
}

static bool iterateString(ObjIterator* iterator, Value* value) {
    assert(iterator->target->type == OBJ_STRING);

    ObjString* string = (ObjString*)iterator->target;

    int capacity = string->length;
    int current = iterator->currentIndex;
    if (current == -1 || capacity == 0 || current >= capacity) return false;

    // Set value to point to the value pre-iteration
    *(value) = indexString(string, current);

    // Get next iteration
    iterator->currentIndex++;
    return true;
}

/**
 * @brief Iterates the iterator if possible
 * 
 * @param iterator A pointer to an iterator object
 * @param value    A pointer to the current value 
 * @return         If the current iterator value is valid
 */
bool iterateObj(ObjIterator* iterator, Value* value) {
    switch (iterator->target->type) {
        case OBJ_SET:    return iterateSet(iterator, value);
        case OBJ_TUPLE:  return iterateTuple(iterator, value);
        case OBJ_STRING: return iterateString(iterator, value);
    }

    return false;
}