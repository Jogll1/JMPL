#ifndef c_jmpl_object_h
#define c_jmpl_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"

// Get type

#define OBJ_TYPE(value)    (AS_OBJ(value)->type)

// Type check

#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_UPVALUE(value)      isObjType(value, OBJ_UPVALUE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_SET(value)          isObjType(value, OBJ_SET)
#define IS_SET_ITERATOR(value) isObjType(value, OBJ_SET_ITERATOR)
#define IS_TUPLE(value)        isObjType(value, OBJ_TUPLE)

// JMPL Object to C object

#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value)))
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_SET(value)          (((ObjSet*)AS_OBJ(value)))
#define AS_SET_ITERATOR(value) (((ObjSetIterator*)AS_OBJ(value)))
#define AS_TUPLE(value)        (((ObjTuple*)AS_OBJ(value)))

/**
 * @brief The type of an Object.
 */
typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_SET,
    OBJ_SET_ITERATOR,
    OBJ_TUPLE
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    bool isReady;
    struct Obj* next;
};

// --- Object Types --- 

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    int arity;
    NativeFn function;
} ObjNative;

struct ObjString {
    Obj obj;
    int length;
    unsigned char* chars;
    uint32_t hash;
};

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

// ------

Obj* allocateObject(size_t size, ObjType type);

ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function, int arity);
ObjString* takeString(unsigned char* chars, int length);
ObjString* copyString(const unsigned char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);

void printJMPLString(ObjString* string);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif