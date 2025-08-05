#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "memory.h"
#include "set.h"
#include "tuple.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

// Tune
#define GC_HEAP_GROW_FACTOR 2

/**
 * @brief Function for dynamic memory reallocation in c_jmpl.
 */
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
        if (vm.bytesAllocated > vm.nextGC) {
            collectGarbage();
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

void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object), true);
    printf("\n");
#endif
    object->isMarked = true;

    // Marking roots using the tricolour abstraction
    if (vm.greyCapacity < vm.greyCount + 1) {
        vm.greyCapacity = GROW_CAPACITY(vm.greyCapacity);
        vm.greyStack = (Obj**)realloc(vm.greyStack, sizeof(Obj*) * vm.greyCapacity);

        if (vm.greyStack == NULL) exit(INTERNAL_SOFTWARE_ERROR); // Exit abruptly - probably should change
    }

    vm.greyStack[vm.greyCount++] = object;
}

void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object), true);
    printf("\n");
#endif

    switch(object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE: {
            markValue(((ObjUpvalue*)object)->closed);
            break;
        }
        case OBJ_SET: {
            ObjSet* set = (ObjSet*)object;
            markSet(set);
            break;
        }
        case OBJ_SET_ITERATOR: {
            ObjSetIterator* iterator = (ObjSetIterator*)object;
            markObject((Obj*)iterator->set);
            break;
        }
        case OBJ_TUPLE: {
            ObjTuple* tuple = (ObjTuple*)object;
            for (int i = 0; i < tuple->size; i++) {
                markValue(tuple->elements[i]);
            }
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
        default:
            break;
    }
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch(object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, object);
            break;
        }
        case OBJ_SET: {
            freeSet((ObjSet*)object);
            break;
        }
        case OBJ_SET_ITERATOR: {
            freeSetIterator((ObjSetIterator*)object);
            break;
        }
        case OBJ_TUPLE: {
            ObjTuple* tuple = (ObjTuple*)object;
            FREE_ARRAY(Value, tuple->elements, tuple->size);
            FREE(ObjTuple, object);
            break;
        }
    }
}

static void markRoots() {
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    markTable(&vm.globals);
    markTable(&vm.strings);
    markCompilerRoots();
}

static void traceReferences() {
    while (vm.greyCount > 0) {
        Obj* object = vm.greyStack[--vm.greyCount];
        blackenObject(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;

    while (object != NULL) {
        if (object->isMarked || !object->isReady) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;

            if(previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings);
    sweep();
    
    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}

void freeObjects() {
    Obj* object = vm.objects;
    
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    free(vm.greyStack);
}