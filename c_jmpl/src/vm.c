#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "vm.h"
#include "object.h"
#include "memory.h"
#include "native.h"
#include "debug.h"

// ToDo: swap this for a pointer
VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const unsigned char* format, ...) {
    // Print the stack trace
    for(int i = 0; i < vm.frameCount; i++) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

        if(function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s\n", function->name->chars);
        }
    }
    
    // List of arbritrary number of arguments
    va_list args;
    va_start(args, format);
    // Prints the arguments
    printf(ANSI_RED "RuntimeError" ANSI_RESET ": ");
    vfprintf(stderr, format, args);
    va_end(args);
    fputs(".\n", stderr);

    resetStack();
}

/**
 * @brief Adds a native function.
 * 
 * @param name     The name of the function in JMPL
 * @param arity    How many parameters it should have
 * @param function The C function that is called
 */
static void defineNative(const unsigned char* name, int arity, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function, arity)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    vm.greyCount = 0;
    vm.greyCapacity = 0;
    vm.greyStack = NULL;

    initTable(&vm.globals);
    initTable(&vm.strings);

    // --- Add native functions ---

    // General purpose
    defineNative("clock", 0, clockNative);

    // Maths library
    defineNative("pi", 0, piNative);

    defineNative("sin", 1, sinNative);
    defineNative("cos", 1, cosNative);
    defineNative("tan", 1, tanNative);
    defineNative("arcsin", 1, arcsinNative);
    defineNative("arccos", 1, arccosNative);
    defineNative("arctan", 1, arctanNative);

    defineNative("max", 2, maxNative);
    defineNative("min", 2, minNative);
    defineNative("floor", 1, floorNative);
    defineNative("ceil", 1, ceilNative);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

/**
 * @brief Returns a value from the stack but doesn't pop it.
 *
 * @param distance How far down from the top of the stack to look, zero being the top
 * @return         The value at that distance on the stack
 */
static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
    if(argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d", closure->function->arity, argCount);
        return false;
    }

    if(vm.frameCount == FRAMES_MAX) {
        runtimeError("Call stack overflow");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

/**
 * @brief Call a callable.
 * 
 * @param callee   The object to call
 * @param argCount The number of arguments to the callable
 */
static bool callValue(Value callee, int argCount) {
    if(IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                ObjNative* objNative = AS_NATIVE(callee);

                if(argCount != objNative->arity) {
                    runtimeError("Expected %d arguments but got %d", objNative->arity, argCount);
                    return false;
                }

                NativeFn native = objNative->function;
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break;
        }
    }

    runtimeError("Can only call functions");
    return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    while(upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if(prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while(vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

/**
 * @brief Determines if a value is false.
 * 
 * @param value The value to determine the Boolean value of
 * 
 * Values are false if they are null, false, 0, or an empty string.
 */
static bool isFalse(Value value) {
    // Return false if null, false, 0, or empty string
    return IS_NULL(value) || 
           (IS_NUMBER(value) && AS_NUMBER(value) == 0) || 
           (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_STRING(value) && AS_CSTRING(value)[0] == '\0');
}

/**
 * @brief Concatenate strings a and b if either are a string.
 * 
 * Pops two strings from the stack and pushes the concatenated
 * string to the stack.
 */
static void concatenate() {
    Value b = peek(0); // Peek second String
    Value a = peek(1); // Peek first String

    ObjString* aString = valueToString(a);
    ObjString* bString = valueToString(b);

    // Length of concatenated string
    int length = aString->length + bString->length;
    // Allocated memory for concatenated string and null terminator
    unsigned char* chars = ALLOCATE(unsigned char, length + 1);

    memcpy(chars, aString->chars, aString->length);
    memcpy(chars + aString->length, bString->chars, bString->length);
    chars[length] = '\0'; // Null terminator

    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE()     (*frame->ip++)
#define READ_SHORT()    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())
// --- Ugly ---
#define BINARY_OP(valueType, op) \
    do { \
        if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)
#define EXPONENT(valueType) \
    do { \
        if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(pow(a, b))); \
    } while (false)
// ---

    while(true) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for(Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        
        disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
#endif

        uint8_t instruction;
        switch(instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NULL: push(NULL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if(!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if(tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_NOT_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(!valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_LESS_EQUAL: BINARY_OP(BOOL_VAL, <=); break;
            case OP_ADD: {
                if(IS_STRING(peek(0)) || IS_STRING(peek(1))) {
                    // Concatenate if at least one operand is a string
                    concatenate();
                } else if(IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    // Else, numerically add
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError("Invalid operand type(s)");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_MOD: {
                if(!IS_INTEGER(peek(0)) || !IS_INTEGER(peek(1))) {
                    runtimeError("Operands must be integers");
                    return INTERPRET_RUNTIME_ERROR;
                } else if (IS_NUMBER(peek(0)) && AS_NUMBER(peek(0)) == 0) {
                    // Return error if divisor is 0
                    runtimeError("Division by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                int b = AS_NUMBER(pop());
                int a = AS_NUMBER(pop());
                push(NUMBER_VAL(a % b));
                break;
            }
            case OP_DIVIDE: {
                if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers");
                    return INTERPRET_RUNTIME_ERROR;
                } else if (IS_NUMBER(peek(0)) && AS_NUMBER(peek(0)) == 0) {
                    // Return error if divisor is 0
                    runtimeError("Division by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a / b));
                break;
            }
            case OP_EXPONENT: EXPONENT(NUMBER_VAL); break;
            case OP_NOT: {   
                push(BOOL_VAL(isFalse(pop()))); 
                break;
            }
            case OP_NEGATE: {
                if(!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }
            case OP_OUT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if(isFalse(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if(!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(OBJ_VAL(closure));
                for(int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if(isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            }
            case OP_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if(vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
#undef EXPONENT
}

InterpretResult interpret(const unsigned char* source) {
    ObjFunction* function = compile(source);
    if(function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));
    
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}