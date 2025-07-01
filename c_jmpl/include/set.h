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

void printSet(ObjSet* set);

#endif