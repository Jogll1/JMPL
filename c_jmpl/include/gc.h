#ifndef c_jmpl_gc_h
#define c_jmpl_gc_h

#include "common.h"
#include "value.h"

#define INITIAL_GC 1024 * 1024

typedef struct Obj Obj;

typedef struct GC {
    Obj* objects;
    size_t bytesAllocated;
    size_t nextGC;

    int greyCount;
    int greyCapacity;
    Obj** greyStack;

    // Stack for temporary values that shouldn't be freed too soon
    int tempCount;
    int tempCapacity;
    Value* tempStack;

    void (*markRoots)(struct GC*, void*);
    void* markRootsArgs;
    void (*fixWeak)(void*);
    void* fixWeakArg;
} GC;

void initGC(GC* gc);
void freeGC(GC* gc);
void pushTemp(GC* gc, Value value);
void popTemp(GC* gc);

#endif