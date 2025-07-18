#ifndef c_jmpl_tuple_h
#define c_jmpl_tuple_h

#include "common.h"
#include "object.h"
#include "valtable.h"

/**
 * @brief The JMPL representation of a Tuple.
 */
typedef struct {
    Obj obj;
    int size;
    Value* elements;
} ObjTuple;

ObjTuple* newTuple(int size);
bool tuplesEqual(ObjTuple* a, ObjTuple* b);
unsigned char* tupleToString(ObjTuple* tuple);
ObjTuple* concatenateTuple(ObjTuple* a, ObjTuple* b);
uint32_t hashTuple(ObjTuple* tuple);

#endif