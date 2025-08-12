#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "memory.h"
#include "set.h"
#include "obj_string.h"
#include "tuple.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

/**
 * @brief Function for dynamic memory reallocation in c_jmpl.
 */
void* reallocate(GC* gc, void* pointer, size_t oldSize, size_t newSize) {
    gc->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(gc);
#endif
        if (gc->bytesAllocated > gc->nextGC) {
            collectGarbage(gc);
        }
    }

    if (newSize == 0) {
        free(pointer);
        pointer = NULL;
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(INTERNAL_SOFTWARE_ERROR);
    return result;
}

void markObject(GC* gc, Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object), true);
    printf("\n");
#endif
    object->isMarked = true;

    // Marking roots using the tricolour abstraction
    if (gc->greyCapacity < gc->greyCount + 1) {
        gc->greyCapacity = GROW_CAPACITY(gc->greyCapacity);
        gc->greyStack = (Obj**)realloc(gc->greyStack, sizeof(Obj*) * gc->greyCapacity);

        if (gc->greyStack == NULL) exit(INTERNAL_SOFTWARE_ERROR); // Exit abruptly - probably should change
    }

    gc->greyStack[gc->greyCount++] = object;
}

void markValue(GC* gc, Value value) {
    if (IS_OBJ(value)) markObject(gc, AS_OBJ(value));
}

static void markArray(GC* gc, ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(gc, array->values[i]);
    }
}

static void blackenObject(GC* gc, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object), true);
    printf("\n");
#endif

    switch(object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject(gc, (Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject(gc, (Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject(gc, (Obj*)function->name);
            markArray(gc, &function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE: {
            markValue(gc, ((ObjUpvalue*)object)->closed);
            break;
        }
        case OBJ_SET: {
            ObjSet* set = (ObjSet*)object;
            for (int i = 0; i < set->capacity; i++) {
                markValue(gc, set->elements[i]);
            }
            break;
        }
        case OBJ_SET_ITERATOR: {
            ObjSetIterator* iterator = (ObjSetIterator*)object;
            markObject(gc, (Obj*)iterator->set);
            break;
        }
        case OBJ_TUPLE: {
            ObjTuple* tuple = (ObjTuple*)object;
            for (int i = 0; i < tuple->size; i++) {
                markValue(gc, tuple->elements[i]);
            }
            break;
        }
        case OBJ_NATIVE:
        case OBJ_UNICODE_STRING:
        default:
            break;
    }
}

static void freeObject(GC* gc, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch(object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(gc, ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(gc, ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(gc, &function->chunk);
            FREE(gc, ObjFunction, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(gc, ObjNative, object);
            break;
        }
        case OBJ_UNICODE_STRING: {
            freeUnicodeString(gc, (ObjUnicodeString*)object);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(gc, ObjUpvalue, object);
            break;
        }
        case OBJ_SET: {
            freeSet(gc, (ObjSet*)object);
            break;
        }
        case OBJ_SET_ITERATOR: {
            freeSetIterator(gc, (ObjSetIterator*)object);
            break;
        }
        case OBJ_TUPLE: {
            ObjTuple* tuple = (ObjTuple*)object;
            FREE_ARRAY(gc, Value, tuple->elements, tuple->size);
            FREE(gc, ObjTuple, object);
            break;
        }
    }
}

static void markRoots(GC* gc) {
    for (int i = 0; i < gc->tempCount; i++) {
        markValue(gc, gc->tempStack[i]);
    }
    
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(gc, *slot);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        markObject(gc, (Obj*)vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(gc, (Obj*)upvalue);
    }

    markValue(gc, vm.impReturnStash);

    markTable(gc, &vm.globals);
    markTable(gc, &vm.strings);
    markCompilerRoots();
}

static void traceReferences(GC* gc) {
    while (gc->greyCount > 0) {
        Obj* object = gc->greyStack[--gc->greyCount];
        blackenObject(gc, object);
    }
}

static void sweep(GC* gc) {
    Obj* previous = NULL;
    Obj* object = gc->objects;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;

            if(previous != NULL) {
                previous->next = object;
            } else {
                gc->objects = object;
            }

            freeObject(gc, unreached);
        }
    }
}

void collectGarbage(GC* gc) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = gc->bytesAllocated;
#endif

    markRoots(gc);
    traceReferences(gc);
    tableRemoveWhite(&vm.strings);
    sweep(gc);
    
    gc->nextGC = gc->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - gc->bytesAllocated, before, gc->bytesAllocated, gc->nextGC);
#endif
}

void freeObjects(GC* gc) {
    Obj* object = gc->objects;
    
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(gc, object);
        object = next;
    }

    free(gc->greyStack);
}