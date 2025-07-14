#ifndef c_jmpl_compiler_h
#define c_jmpl_compiler_h

#include "vm.h"
#include "scanner.h"

typedef struct {
    Scanner scanner;

    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    Upvalue upvalues[UINT8_COUNT];
    int localCount;
    int scopeDepth;

    bool implicitReturn;
} Compiler;

ObjFunction* compile(const unsigned char* source);
void markCompilerRoots();

#endif