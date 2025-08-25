#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "gc.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
#include "table.h"
#include "obj_string.h"
#include "value.h"
#include "vm.h"
#include "../lib/c-stringbuilder/sb.h"

Obj* allocateObject(GC* gc, size_t size, ObjType type, bool isIterable) {
    Obj* object = (Obj*)reallocate(gc, NULL, 0, size);
    object->type = type;
    object->isMarked = false;
    object->isIterable = isIterable;

    object->next = gc->objects;
    gc->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjClosure* newClosure(GC* gc, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(gc, ObjUpvalue*, function->upvalueCount);
    for(int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(gc, ObjClosure, OBJ_CLOSURE, false);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction(GC* gc) {
    ObjFunction* function = ALLOCATE_OBJ(gc, ObjFunction, OBJ_FUNCTION, false);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(GC* gc, NativeFn function, int arity) {
    ObjNative* native = ALLOCATE_OBJ(gc, ObjNative, OBJ_NATIVE, false);
    native->arity = arity;
    native->function = function;
    return native;
}

ObjUpvalue* newUpvalue(GC* gc, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(gc, ObjUpvalue, OBJ_UPVALUE, false);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

ObjModule* newModule(GC* gc, ObjString* name) {
    ObjModule* module = ALLOCATE_OBJ(gc, ObjModule, OBJ_MODULE, false);
    module->name = name;
    initTable(&module->globals);
    return module;
}

static void printFunction(ObjFunction* function) {
    if(function->name == NULL) {
        printf("<script>");
    } else {
        printf("<fn %s>", function->name->utf8);
    }
}

static void printModule(ObjModule* module) {
    if(module->name == NULL) {
        printf("<module>");
    } else {
        printf("<module %s>", module->name->utf8);
    }
}

void printObject(Value value, bool simple) {
    switch(OBJ_TYPE(value)) {
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native>");
            break;
        case OBJ_MODULE:
            printModule(AS_MODULE(value));
            break;
        case OBJ_STRING:
            printJMPLString(AS_STRING(value));
            break;
        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;
        case OBJ_SET:
            if (simple) {
                printf("<set>");
            } else {
                unsigned char* setStr = setToString(AS_SET(value));
                printf("%s", setStr);
                free(setStr);
            }
            break;
        case OBJ_TUPLE:
            if (simple) {
                printf("<tuple>");
            } else {
                unsigned char* tupleStr = tupleToString(AS_TUPLE(value));
                printf("%s", tupleStr);
                free(tupleStr);
            }
            break;
        case OBJ_ITERATOR:
            printf("<iterator>");
            break;
        default: 
            printf("<unknown>");
            return;
    }
}