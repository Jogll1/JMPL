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

typedef struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings; // Table for string interning
    ObjUpvalue* openUpvalues;

    GC gc;

    Value impReturnStash; // Register for storing implicit return value
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();

void push(Value value);
Value pop();

InterpretResult interpret(const unsigned char* source);

#endif