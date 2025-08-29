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

Value indexTuple(ObjTuple* tuple, int index);
ObjTuple* sliceTuple(GC* gc, ObjTuple* tuple, int start, int end);
ObjTuple* concatenateTuple(GC* gc, ObjTuple* a, ObjTuple* b);

void printTuple(ObjTuple* tuple);
unsigned char* tupleToString(ObjTuple* tuple);

#endif