#ifndef c_jmpl_iterator_h
#define c_jmpl_iterator_h

#include "object.h"

/**
 * @brief Object to iterate through another object.
 * 
 * Iterable objects:
 * - Set
 * - Tuple
 * - String
 */
typedef struct {
    Obj obj;
    Obj* target;        // Object to iterate through
    int currentIndex;   // Index of the current value
} ObjIterator;

ObjIterator* newIterator(GC* gc, Obj* target);
void freeIterator(GC* gc, ObjIterator* iterator);
bool iterateObj(ObjIterator* iterator, Value* value);

#endif