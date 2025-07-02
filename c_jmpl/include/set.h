#ifndef c_jmpl_set_h
#define c_jmpl_set_h

#include "common.h"
#include "object.h"
#include "valtable.h"

// /**
//  * @brief The type of a Set.
//  */
// typedef enum {
//     SET_FINITE,
//     SET_INFINITE,
//     SET_LAZY
// } SetType;

/**
 * @brief The JMPL representation of a Set.
 * 
 * For now all sets are hash table wrappers (finite).
 */
typedef struct ObjSet {
    Obj obj;
    ValTable elements;
} ObjSet;

ObjSet* newSet();

bool setInsert(ObjSet* set, Value value);
bool setContains(ObjSet* set, Value value);
bool setRemove(ObjSet* set, Value value);
bool freeSet(ObjSet* set);
bool setsEqual(ObjSet* a, ObjSet* b);

ObjSet* setIntersect(ObjSet* a, ObjSet* b);
ObjSet* setUnion(ObjSet* a, ObjSet* b);

uint32_t hashSet(ObjSet* set);
unsigned char* setToString(ObjSet* set);

#endif