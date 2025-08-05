#ifndef c_jmpl_gc_h
#define c_jmpl_gc_h

#include "value.h"

typedef struct Obj Obj;

#define INTIAL_GC 1024 * 1024
#define GC_HEAP_GROW_FACTOR 2

typedef struct GC {
    Obj* objects;
    size_t bytesAllocated;
    size_t nextGC;

    int greyCount;
    int greyCapacity;
    Obj** greyStack;

    int tempCount;
    int tempCapacity;
    Value* tempStack;
} GC;

void initGC(GC* gc);
void freeGC(GC* gc);

void pushTemp(GC* gc, Value value);
void popTemp(GC* gc);

#endif