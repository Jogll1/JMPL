#ifndef c_jmpl_set_h
#define c_jmpl_set_h

#include "object.h"

#define IS_FINITE_SET(set) (set->type == SET_FINITE)
#define IS_RANGE_SET(set)  (set->type == SET_RANGE)

#define AS_FINITE_SET(set) ((FiniteSet*)set)
#define AS_RANGE_SET(set)  ((RangeSet*)set)

typedef enum {
    SET_FINITE,
    SET_RANGE
} SetType;

/**
 * @brief The JMPL representation of a Set.
 */
typedef struct ObjSet {
    Obj obj;
    SetType type;
} ObjSet;

/**
 * @brief Stores values in a HashSet.
 */
typedef struct FiniteSet {
    ObjSet header;
    int count;
    int capacity;
    Value* elements;
} FiniteSet;

/**
 * @brief Represents a range of values as a Set.
 */
typedef struct {
    ObjSet header;
    int start;
    int end;
    int step;
    int size;
} RangeSet;

// --- FiniteSet ---
FiniteSet* newFiniteSet(GC* gc);

// --- RangeSet ---
RangeSet* newRangeSet(GC* gc, int start, int end, int step);

// --- ObjSet ---
void freeSet(GC* gc, ObjSet* set);
void markSet(GC* gc, ObjSet* set);
size_t getSetSize(ObjSet* set);

bool setInsert(GC* gc, ObjSet* set, Value value);
bool setContains(ObjSet* set, Value value);
bool setsEqual(ObjSet* a, ObjSet* b);

ObjSet* setIntersect(GC* gc, ObjSet* a, ObjSet* b);
ObjSet* setUnion(GC* gc, ObjSet* a, ObjSet* b);
ObjSet* setDifference(GC* gc, ObjSet* a, ObjSet* b);

bool isSubset(GC* gc, ObjSet* a, ObjSet* b);
bool isProperSubset(GC* gc, ObjSet* a, ObjSet* b);

Value getArb(ObjSet* set);

unsigned char* setToString(ObjSet* set);

#endif