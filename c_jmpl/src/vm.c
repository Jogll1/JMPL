#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "vm.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
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
    for (int i = 0; i < vm.frameCount; i++) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", getLine(&function->chunk, instruction));

        if (function->name == NULL) {
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

    vm.impReturnStash = NULL_VAL;

    initTable(&vm.globals);
    initTable(&vm.strings);

    // --- Add native functions ---

    // General purpose
    defineNative("clock", 0, clockNative);

    // I/O
    defineNative("print", 1, printNative);
    defineNative("println", 1, printlnNative);

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
    defineNative("round", 1, roundNative);
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

static int setOmission(bool hasNext) {
    if (hasNext) {
        if (!IS_INTEGER(peek(0)) || !IS_INTEGER(peek(1)) || !IS_INTEGER(peek(2)) || !IS_SET(peek(3))) {
            runtimeError("Expected: {int, int ... int}");
            return INTERPRET_RUNTIME_ERROR; 
        }
    } else {
        if (!IS_INTEGER(peek(0)) || !IS_INTEGER(peek(1)) || !IS_SET(peek(2))) {
            runtimeError("Expected: {int ... int}");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    int last = (int)AS_NUMBER(pop());
    int next;
    if (hasNext) {
        next = (int)AS_NUMBER(pop());
    }
    int first = (int)AS_NUMBER(pop());
    ObjSet* set = AS_SET(pop());
    set->obj.isReady = false;

    int gap = 1;
    if (hasNext) {
        gap = abs(next - first);

        if (gap == 0) {
            runtimeError("Set omission gap cannot be zero");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    if (last > first) {
        for (int i = first; i <= last; i += gap) {
            setInsert(set, NUMBER_VAL(i));
        }   
    } else {
        for (int i = first; i >= last; i -= gap) {
            setInsert(set, NUMBER_VAL(i));
        }   
    }

    set->obj.isReady = true;
    push(OBJ_VAL(set));
    return INTERPRET_OK;
}

static int tupleOmission(bool hasNext) {
    if (hasNext) {
        if (!IS_INTEGER(peek(0)) || !IS_INTEGER(peek(1)) || !IS_INTEGER(peek(2))) {
            runtimeError("Expected: (int, int ... int)");
            return INTERPRET_RUNTIME_ERROR; 
        }
    } else {
        if (!IS_INTEGER(peek(0)) || !IS_INTEGER(peek(1))) {
            runtimeError("Expected: (int ... int)");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    int last = (int)AS_NUMBER(pop());
    int next;
    if (hasNext) {
        next = (int)AS_NUMBER(pop());
    }
    int first = (int)AS_NUMBER(pop());

    int gap = 1;
    if (hasNext) {
        gap = abs(next - first);

        if (gap == 0) {
            runtimeError("Tuple omission gap cannot be zero");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    int arity = abs(first - last);
    if (hasNext) {
        arity = (int)floorl((double)arity / (double)gap);
    } 
    arity++;
    
    ObjTuple* tuple = newTuple(arity);
    tuple->obj.isReady = false;
    int i = 0;
    if (last > first) {
        int i = 0;
        for (int j = first; j <= last; j += gap) {
            tuple->elements[i] = NUMBER_VAL(j);
            i++;
        }   
    } else {
        for (int j = first; j >= last; j -= gap) {
            tuple->elements[i] = NUMBER_VAL(j);
            i++;
        }   
    }

    tuple->obj.isReady = true;
    push(OBJ_VAL(tuple));
    return INTERPRET_OK;
}

/**
 * @brief Determines if a value is false.
 * 
 * @param value The value to determine the Boolean value of
 * 
 * Values are false if they are null, false, 0, empty string, or an empty set.
 */
static bool isFalse(Value value) {
    // Return false if null, false, 0, or empty string
    return IS_NULL(value) || 
           (IS_NUMBER(value) && AS_NUMBER(value) == 0) || 
           (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_STRING(value) && AS_CSTRING(value)[0] == '\0' ||
           (IS_SET(value) && AS_SET(value)->count == 0));
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

/**
 * @brief Concatenate strings a and b if either are a string.
 * 
 * Pops two strings from the stack and pushes the concatenated
 * string to the stack.
 */
static void concatenateString() {
    Value b = pop();
    Value a = pop();

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
    push(OBJ_VAL(result));
}

static int subscript() {
    if (!IS_INTEGER(peek(0))) {
        runtimeError("Tuple index must be an integer");
        return INTERPRET_RUNTIME_ERROR;
    }
    int index = (int)AS_NUMBER(pop());

    Value value = pop();
    if (IS_TUPLE(value)) {
        ObjTuple* tuple = AS_TUPLE(value);
        if (index > tuple->size - 1 || index < 0) {
            runtimeError("Tuple index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        push(tuple->elements[index]);
    } else if (IS_STRING(value)) {
        ObjString* string = AS_STRING(value);
        if (index > string->length - 1 || index < 0) {
            runtimeError("String index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        // Probably make this a function
        int len = 1;
        unsigned char* chars = (char*)malloc(len + 1);
        if (!chars) {
            runtimeError("Out of memory :(");
            return INTERNAL_SOFTWARE_ERROR;
        }
        strncpy(chars, string->chars + index, len);
        chars[len] = '\0';

        push(OBJ_VAL(copyString(chars, len)));
        free(chars);

    } else {
        runtimeError("Object cannot be subscripted");
        return INTERPRET_RUNTIME_ERROR;
    }

    return INTERPRET_OK;
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE()     (*frame->ip++)
#define READ_SHORT()    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_SHORT()])
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
                if (!tableGet(&vm.globals, name, &value)) {
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
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'", name->chars);
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
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_LESS_EQUAL: BINARY_OP(BOOL_VAL, <=); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
                    // Concatenate if at least one operand is a string
                    concatenateString();
                } else if (IS_TUPLE(peek(0)) && IS_TUPLE(peek(1))) {
                    ObjTuple* b = AS_TUPLE(pop());
                    ObjTuple* a = AS_TUPLE(pop());
                    push(OBJ_VAL(concatenateTuple(a, b)));
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
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
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
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
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
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_STASH: {
                vm.impReturnStash = pop();
                break;
            }
            case OP_SET_CREATE: {
                ObjSet* set = newSet();
                push(OBJ_VAL(set));
                break;
            }
            case OP_SET_INSERT: {
                Value value = pop();
                Value setVal = pop();
                ObjSet* set = AS_SET(setVal);
                setInsert(set, value);
                push(setVal);
                break;
            }
            case OP_SET_INSERT_2: { // Inserts two values off the stack - not great and should probably be the same as OP_SET_INSERT
                Value valueA = pop();
                Value valueB = pop();
                Value setVal = pop();
                ObjSet* set = AS_SET(setVal);
                setInsert(set, valueB);
                setInsert(set, valueA);
                push(setVal);
                break;
            }
            case OP_SET_OMISSION: {
                bool omissionParameter = READ_BYTE();
                int status = setOmission(omissionParameter);
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
            case OP_SET_INTERSECT: SET_OP(OBJ_VAL, setIntersect); break;
            case OP_SET_UNION: SET_OP(OBJ_VAL, setUnion); break;
            case OP_SET_DIFFERENCE: SET_OP(OBJ_VAL, setDifference); break;
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
                ObjTuple* tuple = newTuple(arity);
                for (int i = arity - 1; i >= 0; i--) {
                    tuple->elements[i] = pop();
                }
                push(OBJ_VAL(tuple));
                break;
            }
            case OP_TUPLE_OMISSION: {
                bool omissionParameter = READ_BYTE();
                int status = tupleOmission(omissionParameter);
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_SUBSCRIPT: {
                int status = subscript();
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_CREATE_ITERATOR: {
                if (!IS_SET(peek(0))) {
                    runtimeError("Generator must iterate over a set");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSet* set = AS_SET(pop());
                ObjSetIterator* iterator = newSetIterator(set);
                push(OBJ_VAL(iterator));
                break;
            }
            case OP_ITERATE: {
                if (!IS_SET_ITERATOR(peek(0))) {
                    runtimeError("(Internal) Missing iterator");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSetIterator* iterator = AS_SET_ITERATOR(pop());
                Value value;
                bool hasCurrentVal = iterateSetIterator(iterator, &value);
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
#undef BINARY_OP
#undef SET_OP
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