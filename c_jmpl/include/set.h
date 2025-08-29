#ifndef c_jmpl_set_h
#define c_jmpl_set_h

#include "object.h"
#include "hash.h"

typedef struct {
    Value key;
    hash_t hash;
} SetEntry;

/**
 * @brief The JMPL representation of a Set.
 */
typedef struct ObjSet {
    Obj obj;
    SetEntry* entries;
    size_t count;
    size_t capacity;
} ObjSet;

static inline Value getSetValue(ObjSet* set, size_t index) {
    return set->entries[index].key;
}

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

void printSet(ObjSet* set);
unsigned char* setToString(ObjSet* set);

#endif