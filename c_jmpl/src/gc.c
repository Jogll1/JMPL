#include <assert.h>
#include <stdlib.h>

#include "gc.h"
#include "value.h"
#include "memory.h"

void initGC(GC* gc) {
    gc->objects = NULL;
    gc->bytesAllocated = 0;
    gc->nextGC = INTIAL_GC;

    gc->greyCount = 0;
    gc->greyCapacity = 0;
    gc->greyStack = NULL;

    gc->tempCount = 0;
    gc->tempCapacity = 0;
    gc->tempStack = NULL;
}

void freeGC(GC* gc) {
    freeObjects(gc);
    gc->objects = NULL;
    free(gc->tempStack);
}

void pushTemp(GC* gc, Value value) {
    if (gc->tempCapacity < gc->tempCount + 1) {
        gc->tempCapacity = GROW_CAPACITY(gc->tempCapacity);
        gc->tempStack = (Value*)realloc(gc->tempStack, sizeof(Value) * gc->tempCapacity);
        assert(gc->tempStack != NULL);
    }

    gc->tempStack[gc->tempCount++] = value;
}

Value popTemp(GC* gc) {
    assert(gc->tempCount > 0);
    return gc->tempStack[gc->tempCount--];
}