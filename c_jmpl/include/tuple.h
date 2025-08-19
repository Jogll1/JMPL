#ifndef c_jmpl_tuple_h
#define c_jmpl_tuple_h

#include "value.h"

/**
 * @brief The JMPL representation of a Tuple.
 */
typedef struct {
    Obj obj;
    size_t size;
    Value* elements;
} ObjTuple;

ObjTuple* newTuple(GC* gc, size_t size);
bool tuplesEqual(ObjTuple* a, ObjTuple* b);
ObjTuple* sliceTuple(GC* gc, ObjTuple* tuple, size_t start, size_t end);
unsigned char* tupleToString(ObjTuple* tuple);
ObjTuple* concatenateTuple(GC* gc, ObjTuple* a, ObjTuple* b);

#endif