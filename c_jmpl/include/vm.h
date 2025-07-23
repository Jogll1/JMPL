#ifndef c_jmpl_vm_h
#define c_jmpl_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "gc.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    ValueArray args;

    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    ValueArray globalSlots;
    Table strings;
    ObjString* initString;
    ObjUpvalue* openUpvalues;

    Value impReturnStash; // Register for storing implicit return value

    GC gc;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);

void push(VM* vm, Value value);
Value pop(VM* vm);

InterpretResult interpret(VM* vm, const unsigned char* source);
#endif