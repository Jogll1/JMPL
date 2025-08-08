#ifndef c_jmpl_set_h
#define c_jmpl_set_h

#include "object.h"

/**
 * @brief The JMPL representation of a Set.
 */
typedef struct ObjSet {
    Obj obj;
    int count;
    int capacity;
    Value* elements;
} ObjSet;

/**
 * @brief Object to iterate through a Set.
 */
typedef struct {
    Obj obj;
    ObjSet* set;      // Set to iterate through
    int currentIndex; // Index of the current set value
} ObjSetIterator;

typedef struct {
    int start;
    int end;
    int current;
    int step;
} RangeSet;

// --- ObjSet ---
ObjSet* newSet(GC* gc);
void freeSet(GC* gc, ObjSet* set);

bool setInsert(GC* gc, ObjSet* set, Value value);
bool setContains(ObjSet* set, Value value);
bool setsEqual(ObjSet* a, ObjSet* b);

ObjSet* setIntersect(GC* gc, ObjSet* a, ObjSet* b);
ObjSet* setUnion(GC* gc, ObjSet* a, ObjSet* b);
ObjSet* setDifference(GC* gc, ObjSet* a, ObjSet* b);

bool isSubset(ObjSet* a, ObjSet* b);
bool isProperSubset(ObjSet* a, ObjSet* b);

Value getArb(ObjSet* set);

uint32_t hashSet(ObjSet* set);
unsigned char* setToString(ObjSet* set);

// --- ObjSetIterator ---
ObjSetIterator* newSetIterator(GC* gc, ObjSet* set);
void freeSetIterator(GC* gc, ObjSetIterator* iterator);
bool iterateSetIterator(ObjSetIterator* iterator, Value* value);

#endif