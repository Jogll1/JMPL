#ifndef c_jmpl_set_h
#define c_jmpl_set_h

#include "common.h"
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
ObjSet* newSet();
void freeSet(ObjSet* set);

bool setInsert(ObjSet* set, Value value);
bool setContains(ObjSet* set, Value value);

bool setsEqual(ObjSet* a, ObjSet* b);
ObjSet* setIntersect(ObjSet* a, ObjSet* b);
ObjSet* setUnion(ObjSet* a, ObjSet* b);
ObjSet* setDifference(ObjSet* a, ObjSet* b);
bool isSubset(ObjSet* a, ObjSet* b);
bool isProperSubset(ObjSet* a, ObjSet* b);

Value getArb(ObjSet* set);

uint32_t hashSet(ObjSet* set);
unsigned char* setToString(ObjSet* set);

// --- ObjSetIterator ---
ObjSetIterator* newSetIterator(ObjSet* set);
void freeSetIterator(ObjSetIterator* iterator);
bool iterateSetIterator(ObjSetIterator* iterator, Value* value);

#endif