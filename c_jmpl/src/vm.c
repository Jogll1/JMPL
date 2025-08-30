#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "vm.h"
#include "object.h"
#include "obj_string.h"
#include "set.h"
#include "tuple.h"
#include "memory.h"
#include "native.h"
#include "debug.h"
#include "utils.h"
#include "gc.h"
#include "iterator.h"

// ToDo: swap this for a pointer
VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const unsigned char* format, ...) {
    // Print the stack trace
    for (int i = 0; i < vm.frameCount; i++) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", getLine(&function->chunk, instruction));

        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s\n", function->name->utf8);
        }
    }
    
    // List of arbritrary number of arguments
    va_list args;
    va_start(args, format);
    // Prints the arguments
    fprintf(stderr, ANSI_RED "RuntimeError" ANSI_RESET ": ");
    vfprintf(stderr, format, args);
    va_end(args);
    fputs(".\n", stderr);

    resetStack();
}

void initVM() {
    resetStack();
    initGC(&vm.gc);

    vm.impReturnStash = NULL_VAL;

    initTable(&vm.globals);
    initTable(&vm.strings);
    initTable(&vm.modules);

    // --- Load core library ---
    loadModule(defineCoreLibrary());
}

void freeVM() {
    freeTable(&vm.gc, &vm.globals);
    freeTable(&vm.gc, &vm.strings);
    freeTable(&vm.gc, &vm.modules);
    freeGC(&vm.gc);
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
        if (closure->function->arity != 1) {
            runtimeError("Expected %d arguments but got %d", closure->function->arity, argCount);
        } else {
            runtimeError("Expected %d argument but got %d", closure->function->arity, argCount);
        }
        return false;
    }

    if(vm.frameCount == FRAMES_MAX) {
        runtimeError("(Internal) Call stack overflow");
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
                    if (objNative->arity != 1) {
                        runtimeError("Expected %d arguments but got %d", objNative->arity, argCount);
                    } else {
                        runtimeError("Expected %d argument but got %d", objNative->arity, argCount);
                    }
                    return false;
                }

                NativeFn native = objNative->function;
                Value result = native(&vm, argCount, vm.stackTop - argCount);
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

    ObjUpvalue* createdUpvalue = newUpvalue(&vm.gc, local);
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

static void setInsertN(int count) {
    // Uses the temp stack to protect values about to be inserted from being freed too soon
    for (int i = 0; i < count; i++) {
        Value value = pop();
        pushTemp(&vm.gc, value);
    }
    
    ObjSet* set = AS_SET(pop());

    for (int i = 0; i < count; i++) {
        setInsert(&vm.gc, set, vm.gc.tempStack[vm.gc.tempCount - 1]);
        popTemp(&vm.gc);
    }
    
    push(OBJ_VAL(set));
}

/**
 * @brief Create an omission set or tuple.
 * 
 * @param isSet   If true -> set, if false -> tuple
 * @param hasNext If there is a 'step' value
 * @return        If the operation succeeded
 * 
 * Can be [int, int ... int] or [char, char ... char].
 */
static InterpretResult omission(bool isSet, bool hasNext) {
    bool isIntOmission = IS_INTEGER(peek(0)) && IS_INTEGER(peek(1)) && (!hasNext || IS_INTEGER(peek(2)));
    bool isCharOmission = IS_CHAR(peek(0)) && IS_CHAR(peek(1)) && (!hasNext || IS_CHAR(peek(2)));
    if (!isIntOmission && !isCharOmission) {
        runtimeError("Terms of an omission operation must be all integers or all characters");
        return INTERPRET_RUNTIME_ERROR; 
    }

    int last = (int)(isCharOmission ? AS_CHAR(pop()) : AS_NUMBER(pop()));
    int next;
    if (hasNext) {
        next =  (int)(isCharOmission ? AS_CHAR(pop()) : AS_NUMBER(pop()));
    }
    int first =  (int)(isCharOmission ? AS_CHAR(pop()) : AS_NUMBER(pop()));

    int gap = 1;
    if (hasNext) {
        gap = abs(next - first);

        if (gap == 0) {
            runtimeError("Omission gap cannot be zero");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    int size = (int)floorl((double)abs(first - last) / (double)gap) + 1;

#define CAST(num) (isCharOmission ? CHAR_VAL(num) : NUMBER_VAL(num))

    if (isSet) {
        ObjSet* set = AS_SET(pop());
        pushTemp(&vm.gc, OBJ_VAL(set));

        if (last > first) {
            for (int i = first; i <= last; i += gap) {
                setInsert(&vm.gc, set, CAST(i));
            }   
        } else {
            for (int i = first; i >= last; i -= gap) {
                setInsert(&vm.gc, set, CAST(i));
            }   
        }

        popTemp(&vm.gc);
        push(OBJ_VAL(set));
    } else {
        ObjTuple* tuple = newTuple(&vm.gc, size);
        
        int i = 0;
        if (last > first) {
            int i = 0;
            for (int j = first; j <= last; j += gap) {
                tuple->elements[i] = CAST(j);
                i++;
            }   
        } else {
            for (int j = first; j >= last; j -= gap) {
                tuple->elements[i] = CAST(j);
                i++;
            }   
        }

        push(OBJ_VAL(tuple));
    }

#undef CAST

    return INTERPRET_OK;
}

/**
 * @brief Determines if a value is falsey.
 * 
 * @param value The value to determine the Boolean value of
 * 
 * Values are false if they are null, false, 0, empty string, empty set, or an empty tuple.
 */
static bool isFalse(Value value) {
    // Return false if null, false, 0, or empty string
    return IS_NULL(value) || 
           (IS_NUMBER(value) && AS_NUMBER(value) == 0) || 
           (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_STRING(value) && AS_CSTRING(value)[0] == '\0') ||
           (IS_SET(value) && AS_SET(value)->count == 0) ||
           (IS_TUPLE(value) && AS_TUPLE(value)->size == 0);
}

static int getSize(Value value) {
#ifdef NAN_BOXING
    if (IS_NUMBER(value)) {
        return fabs(AS_NUMBER(value));
    } else if (IS_OBJ(value)) {
        switch (AS_OBJ(value)->type) {
            case OBJ_STRING: return AS_STRING(value)->length;
            case OBJ_SET:    return AS_SET(value)->count;
            case OBJ_TUPLE:  return AS_TUPLE(value)->size;
            default:         return -1;
        }
    }
#else
    switch (value.type) {
        case VAL_NUMBER:
            return fabs(AS_NUMBER(value));
        case VAL_OBJ:
            switch (AS_OBJ(value)->type) {
                case OBJ_STRING: return AS_STRING(value)->length;
                case OBJ_SET:    return AS_SET(value)->count;
                case OBJ_TUPLE:  return AS_TUPLE(value)->size;
                default:         return -1;
            }
            break;
        case VAL_NULL:
        case VAL_BOOL:
        default: break;
    }
#endif

    return -1;
}

static InterpretResult indexObj() {
    if (!IS_INTEGER(peek(0))) {
        runtimeError("Index must be an integer");
        return INTERPRET_RUNTIME_ERROR;
    }
    int index = (signed int)AS_NUMBER(pop());

    Value value = pop();
    if (IS_TUPLE(value)) {
        ObjTuple* tuple = AS_TUPLE(value);
        int intLength = (int)tuple->size;
        if (index < -intLength || index >= intLength) {
            runtimeError("Tuple index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        push(indexTuple(tuple, index));
    } else if (IS_STRING(value)) {
        ObjString* string = AS_STRING(value);
        int intLength = (int)string->length;
        if (index < -intLength || index >= intLength) {
            runtimeError("String index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        push(indexString(string, index));
    } else {
        runtimeError("Object cannot be indexed");
        return INTERPRET_RUNTIME_ERROR;
    }

    return INTERPRET_OK;
}

static InterpretResult sliceObj() {
    if (!IS_INTEGER(peek(0)) && !IS_NULL(peek(0)) || !IS_INTEGER(peek(1)) && !IS_NULL(peek(1))) {
        runtimeError("Slice index must be an integer or null");
        return INTERPRET_RUNTIME_ERROR;
    }

    Value end = pop();
    Value start = pop();
    int startIndex = IS_NULL(start) ? 0 : (signed int)AS_NUMBER(start);
    int endIndex;

    Value value = pop();
    if (IS_TUPLE(value)) {
        ObjTuple* tuple = AS_TUPLE(value);
        int intLength = (int)tuple->size;

        endIndex = IS_NULL(end) ? tuple->size - 1 : (signed int)AS_NUMBER(end);
        
        if (startIndex < -intLength || endIndex < -intLength) {
            runtimeError("Tuple slice index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        push(OBJ_VAL(sliceTuple(&vm.gc, tuple, startIndex, endIndex)));
    } else if (IS_STRING(value)) {
        ObjString* string = AS_STRING(value);
        int intLength = (int)string->length;

        endIndex = IS_NULL(end) ? string->length - 1 : (signed int)AS_NUMBER(end);

        if (startIndex < -intLength || endIndex < -intLength) {
            runtimeError("String slice index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        push(OBJ_VAL(sliceString(&vm.gc, string, startIndex, endIndex)));
    } else {
        runtimeError("Object cannot be indexed");
        return INTERPRET_RUNTIME_ERROR;
    }

    return INTERPRET_OK;
}

static Value importModule(ObjString* path) {
    pushTemp(&vm.gc, OBJ_VAL(path));

    // Resolve path name
    unsigned char absolutePath[MAX_PATH_SIZE];
    if (!getAbsolutePath(path->utf8, absolutePath)) {
        // Check if its a built in module
        if (strcmp(path->utf8, "math") == 0) {
            return OBJ_VAL(defineMathLibrary());
        } else {
            runtimeError("Could not resolve module at '%s'", absolutePath);
            return NULL_VAL;
        }
    }

    // Get the module name (file path without the extension)
    unsigned char fileName[MAX_PATH_SIZE];
    getFileName(path->utf8, fileName, MAX_PATH_SIZE);
    ObjString* moduleName = copyString(&vm.gc, fileName, strlen(fileName));

    // Check if module is already loaded
    Value cached;
    if (tableGet(&vm.modules, moduleName, &cached)) {
        return cached;
    }

    // Create new module
    pushTemp(&vm.gc, OBJ_VAL(moduleName));
    ObjModule* module = newModule(&vm.gc, moduleName);
    tableSet(&vm.gc, &vm.modules, moduleName, OBJ_VAL(module));
    popTemp(&vm.gc);

    // Read library source and compile
    unsigned char* moduleSource = readFile(path->utf8);
    ObjFunction* function = compile(&vm.gc, moduleSource);
    function->name = moduleName;
    free(moduleSource);
    
    if (function == NULL) {
        runtimeError("Could not compile module");
        return NULL_VAL;
    }

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(&vm.gc, function);
    pop();

    popTemp(&vm.gc); // Path

    return OBJ_VAL(closure);
}

static InterpretResult run() {
    register CallFrame* frame;

#define READ_BYTE()     (*frame->ip++)
#define READ_SHORT()    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())
#define LOAD_FRAME()    (frame = &vm.frames[vm.frameCount - 1])
// --- Ugly ---
#define ORDER_OP(op) \
    do { \
        if (!(IS_NUMBER(peek(0)) || IS_CHAR(peek(0))) || !(IS_NUMBER(peek(1)) || IS_CHAR(peek(1)))) { \
            runtimeError("Operands must be numbers or characters"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        Value vb = pop(); \
        Value va = pop(); \
        double b = IS_CHAR(vb) ? AS_NUMBER(AS_CHAR(vb)) : AS_NUMBER(vb); \
        double a = IS_CHAR(va) ? AS_NUMBER(AS_CHAR(va)) : AS_NUMBER(va); \
        push(BOOL_VAL(a op b)); \
    } while (false)

#define SET_OP_GC(valueType, setFunction) \
    do { \
        if (!IS_SET(peek(0)) || !IS_SET(peek(1))) { \
            runtimeError("Operands must be sets"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        ObjSet* setB = AS_SET(pop()); \
        ObjSet* setA = AS_SET(pop()); \
        pushTemp(&vm.gc, OBJ_VAL(setA)); \
        pushTemp(&vm.gc, OBJ_VAL(setB)); \
        push(valueType(setFunction(&vm.gc, setA, setB))); \
        popTemp(&vm.gc); \
        popTemp(&vm.gc); \
    } while (false)

#define SET_OP(valueType, setFunction) \
    do { \
        if (!IS_SET(peek(0)) || !IS_SET(peek(1))) { \
            runtimeError("Operands must be sets"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        ObjSet* setB = AS_SET(pop()); \
        ObjSet* setA = AS_SET(pop()); \
        push(valueType(setFunction(setA, setB))); \
    } while (false)
// ---

#ifdef DEBUG_TRACE_EXECUTION
    #define TRACE_EXECUTION() \
        printStack(vm.stack, vm.stackTop); \
        disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
#else 
    #define TRACE_EXECUTION() do {} while (false)
#endif

    LOAD_FRAME();

    while(true) {
        TRACE_EXECUTION();

        register uint8_t instruction;
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
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'", name->utf8);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.gc, &vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.gc, &vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'", name->utf8);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                
                if (IS_BOOL(b) || IS_BOOL(a)) {
                    // Use truth values
                    push(BOOL_VAL(isFalse(b) == isFalse(a)));
                } else {
                    push(BOOL_VAL(valuesEqual(a, b)));
                }

                break;
            }
            case OP_NOT_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(!valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: ORDER_OP(>); break;
            case OP_GREATER_EQUAL: ORDER_OP(>=); break;
            case OP_LESS: ORDER_OP(<); break;
            case OP_LESS_EQUAL: ORDER_OP(<=); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
                    // Concatenate if at least one operand is a string
                    Value b = pop();
                    Value a = pop();
                    push(OBJ_VAL(concatenateStringsHelper(&vm.gc, a, b)));
                } else if (IS_TUPLE(peek(0)) && IS_TUPLE(peek(1))) {
                    ObjTuple* b = AS_TUPLE(pop());
                    ObjTuple* a = AS_TUPLE(pop());
                    push(OBJ_VAL(concatenateTuple(&vm.gc, a, b)));
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
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
            case OP_SUBTRACT: {
                if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a - b));
                break;
            }
            case OP_MULTIPLY: {
                if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a * b));
                break;
            }
            case OP_MOD: {
                if (!IS_INTEGER(peek(0)) || !IS_INTEGER(peek(1))) {
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
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
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
            case OP_EXPONENT: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(pow(a, b)));
                break;
            }
            case OP_NOT: {   
                push(BOOL_VAL(isFalse(pop()))); 
                break;
            }
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalse(peek(0))) frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE_2: { // Pops the condition always. Couldn't think of anything better
                uint16_t offset = READ_SHORT();
                if (isFalse(peek(0))) {
                    frame->ip += offset;
                    pop();
                }
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                LOAD_FRAME();
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(&vm.gc, function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
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
                bool implicitReturn = READ_BYTE();
                Value result;
                if (implicitReturn) {
                    result = vm.impReturnStash;
                    vm.impReturnStash = NULL_VAL;
                } else {
                    result = pop();
                }

                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                LOAD_FRAME();
                break;
            }
            case OP_STASH: {
                vm.impReturnStash = pop();
                break;
            }
            case OP_SET_CREATE: {
                ObjSet* set = newSet(&vm.gc);
                push(OBJ_VAL(set));
                break;
            }
            case OP_SET_INSERT: {
                uint8_t count = READ_BYTE();
                assert(count > 0);
                setInsertN(count);
                break;
            }
            case OP_SET_OMISSION: {
                bool omissionParameter = READ_BYTE();
                InterpretResult status = omission(true, omissionParameter);
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_SET_IN: {
                if (!IS_SET(peek(0))) {
                    runtimeError("Right hand operand must be a set");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSet* set = AS_SET(pop());
                Value value = pop();
                push(BOOL_VAL(setContains(set, value)));
                break;
            }
            case OP_SET_INTERSECT: SET_OP_GC(OBJ_VAL, setIntersect); break;
            case OP_SET_UNION: SET_OP_GC(OBJ_VAL, setUnion); break;
            case OP_SET_DIFFERENCE: SET_OP_GC(OBJ_VAL, setDifference); break;
            case OP_SUBSET: SET_OP(BOOL_VAL, isProperSubset); break;
            case OP_SUBSETEQ: SET_OP(BOOL_VAL, isSubset); break;
            case OP_SIZE: {
                int size = getSize(pop());
                if (size == -1) {
                    runtimeError("Invalid operand type");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(size));
                break;
            }
            case OP_CREATE_TUPLE: {
                int arity = READ_BYTE();
                ObjTuple* tuple = newTuple(&vm.gc, arity);
                for (int i = arity - 1; i >= 0; i--) {
                    tuple->elements[i] = pop();
                }
                push(OBJ_VAL(tuple));
                break;
            }
            case OP_TUPLE_OMISSION: {
                bool omissionParameter = READ_BYTE();
                InterpretResult status = omission(false, omissionParameter);
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_SUBSCRIPT: {
                bool isSlice = READ_BYTE();
                InterpretResult status;
                if (isSlice) {
                    status = sliceObj();
                } else {
                    status = indexObj();
                }
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_CREATE_ITERATOR: {
                if (!IS_OBJ(peek(0)) || !AS_OBJ(peek(0))->isIterable) {
                    runtimeError("Generator must iterate over a set, tuple, or a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Obj* target = AS_OBJ(pop());
                ObjIterator* iterator = newIterator(&vm.gc, target);
                push(OBJ_VAL(iterator));
                break;
            }
            case OP_ITERATE: {
                if (!IS_ITERATOR(peek(0))) {
                    runtimeError("(Internal) Missing iterator");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjIterator* iterator = AS_ITERATOR(pop());
                Value value;
                bool hasCurrentVal = iterateObj(iterator, &value);
                if (hasCurrentVal) push(value);
                push(BOOL_VAL(hasCurrentVal));
                break;
            }
            case OP_ARB: {
                if (!IS_SET(peek(0))) {
                    runtimeError("Expected set after some keyword");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSet* set = AS_SET(pop());
                push(getArb(set));
                break;
            }
            case OP_IMPORT_LIB: {
                ObjString* path = READ_STRING();
                push(importModule(path));

                if (IS_CLOSURE(peek(0))) {
                    ObjClosure* closure = AS_CLOSURE(pop());
                    call(closure, 0);
                    LOAD_FRAME();
                } else if (IS_MODULE(peek(0))) {
                    // Built-in or cached
                    ObjModule* module = AS_MODULE(pop());
                    loadModule(module);
                } else {
                    // Resolve error
                    pop();
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            default: {
                runtimeError("(Internal) Invalid Opcode");
                return INTERPRET_RUNTIME_ERROR;
            }
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef LOAD_FRAME
#undef SET_OP_GC
#undef SET_OP
}

InterpretResult interpret(const unsigned char* source) {
    ObjFunction* function = compile(&vm.gc, source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));
    
    ObjClosure* closure = newClosure(&vm.gc, function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}