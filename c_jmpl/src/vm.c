#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "compiler.h"
#include "vm.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
#include "memory.h"
#include "native.h"
#include "debug.h"
#include "gc.h"

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = NULL;
}

static void runtimeError(VM* vm, const unsigned char* format, ...) {
    // Print the stack trace
    for (int i = 0; i < vm->frameCount; i++) {
        CallFrame* frame = &vm->frames[i];
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

    resetStack(vm);
}

/**
 * @brief Adds a native function.
 * 
 * @param name     The name of the function in JMPL
 * @param arity    How many parameters it should have
 * @param function The C function that is called
 */
static void defineNative(VM* vm, const unsigned char* name, int arity, NativeFn function) {
    push(vm, OBJ_VAL(copyString(&vm->gc, &vm->strings, name, (int)strlen(name))));
    push(vm, OBJ_VAL(newNative(&vm->gc, function, arity)));
    tableSet(&vm->gc, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
}

void initVM(VM* vm) {
    resetStack(vm);
    vm->gc.objects = NULL;
    vm->gc.bytesAllocated = 0;
    vm->gc.nextGC = 1024 * 1024;

    vm->gc.greyCount = 0;
    vm->gc.greyCapacity = 0;
    vm->gc.greyStack = NULL;

    vm->impReturnStash = NULL_VAL;

    initTable(&vm->globals);
    initTable(&vm->strings);

    // --- Add native functions ---

    // General purpose
    defineNative(vm, "clock", 0, clockNative);

    // I/O
    defineNative(vm, "print", 1, printNative);
    defineNative(vm, "println", 1, printlnNative);

    // Maths library
    defineNative(vm, "pi", 0, piNative);
    defineNative(vm, "sin", 1, sinNative);
    defineNative(vm, "cos", 1, cosNative);
    defineNative(vm, "tan", 1, tanNative);
    defineNative(vm, "arcsin", 1, arcsinNative);
    defineNative(vm, "arccos", 1, arccosNative);
    defineNative(vm, "arctan", 1, arctanNative);
    defineNative(vm, "max", 2, maxNative);
    defineNative(vm, "min", 2, minNative);
    defineNative(vm, "floor", 1, floorNative);
    defineNative(vm, "ceil", 1, ceilNative);
    defineNative(vm, "round", 1, roundNative);
}

void freeVM(VM* vm) {
    freeValueArray(&vm->gc, &vm->args);
    freeTable(&vm->gc, &vm->globals);
    freeValueArray(&vm->gc, &vm->globalSlots);
    freeTable(&vm->gc, &vm->strings);
    vm->initString = NULL;
    freeGC(&vm->gc);
}

void push(VM* vm, Value value) {
    assert(vm->stackTop < vm->stack + STACK_MAX);
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(VM* vm) {
    assert(vm->stackTop > vm->stack);
    vm->stackTop--;
    return *vm->stackTop;
}

static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

static bool call(VM* vm, ObjClosure* closure, int argCount) {
    if(argCount != closure->function->arity) {
        runtimeError(vm, "Expected %d arguments but got %d", closure->function->arity, argCount);
        return false;
    }

    if(vm->frameCount == FRAMES_MAX) {
        runtimeError(vm, "(Internal) Call stack overflow");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm->stackTop - argCount - 1;
    return true;
}

/**
 * @brief Call a callable.
 * 
 * @param callee   The object to call
 * @param argCount The number of arguments to the callable
 */
static bool callValue(VM* vm,Value callee, int argCount) {
    if(IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                ObjNative* objNative = AS_NATIVE(callee);

                if(argCount != objNative->arity) {
                    runtimeError(vm, "Expected %d arguments but got %d", objNative->arity, argCount);
                    return false;
                }

                NativeFn native = objNative->function;
                Value result = native(argCount, vm->stackTop - argCount);
                vm->stackTop -= argCount + 1;
                push(vm, result);
                return true;
            }
            default:
                break;
        }
    }

    runtimeError(vm, "Can only call functions");
    return false;
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm->openUpvalues;
    while(upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(&vm->gc, local);
    createdUpvalue->next = upvalue;

    if(prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(VM* vm, Value* last) {
    while(vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

static int setOmission(VM* vm, bool hasNext) {
    if (hasNext) {
        if (!IS_INTEGER(peek(vm, 0)) || !IS_INTEGER(peek(vm, 1)) || !IS_INTEGER(peek(vm, 2)) || !IS_SET(peek(vm, 3))) {
            runtimeError(vm, "Expected: {int, int ... int}");
            return INTERPRET_RUNTIME_ERROR; 
        }
    } else {
        if (!IS_INTEGER(peek(vm, 0)) || !IS_INTEGER(peek(vm, 1)) || !IS_SET(peek(vm, 2))) {
            runtimeError(vm, "Expected: {int ... int}");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    int last = (int)AS_NUMBER(pop(vm));
    int next;
    if (hasNext) {
        next = (int)AS_NUMBER(pop(vm));
    }
    int first = (int)AS_NUMBER(pop(vm));
    ObjSet* set = AS_SET(pop(vm));

    int gap = 1;
    if (hasNext) {
        gap = abs(next - first);

        if (gap == 0) {
            runtimeError(vm, "Set omission gap cannot be zero");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    if (last > first) {
        for (int i = first; i <= last; i += gap) {
            setInsert(&vm->gc, set, NUMBER_VAL(i));
        }   
    } else {
        for (int i = first; i >= last; i -= gap) {
            setInsert(&vm->gc, set, NUMBER_VAL(i));
        }   
    }

    push(vm, OBJ_VAL(set));
    return INTERPRET_OK;
}

static int tupleOmission(VM* vm, bool hasNext) {
    if (hasNext) {
        if (!IS_INTEGER(peek(vm, 0)) || !IS_INTEGER(peek(vm, 1)) || !IS_INTEGER(peek(vm, 2))) {
            runtimeError(vm, "Expected: (int, int ... int)");
            return INTERPRET_RUNTIME_ERROR; 
        }
    } else {
        if (!IS_INTEGER(peek(vm, 0)) || !IS_INTEGER(peek(vm, 1))) {
            runtimeError(vm, "Expected: (int ... int)");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    int last = (int)AS_NUMBER(pop(vm));
    int next;
    if (hasNext) {
        next = (int)AS_NUMBER(pop(vm));
    }
    int first = (int)AS_NUMBER(pop(vm));

    int gap = 1;
    if (hasNext) {
        gap = abs(next - first);

        if (gap == 0) {
            runtimeError(vm, "Tuple omission gap cannot be zero");
            return INTERPRET_RUNTIME_ERROR; 
        }
    }

    int arity = abs(first - last);
    if (hasNext) {
        arity = (int)floorl((double)arity / (double)gap);
    } 
    arity++;
    
    ObjTuple* tuple = newTuple(&vm->gc, arity);
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

    push(vm, OBJ_VAL(tuple));
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

static int subscript(VM* vm) {
    if (!IS_INTEGER(peek(vm, 0))) {
        runtimeError(vm, "Tuple index must be an integer");
        return INTERPRET_RUNTIME_ERROR;
    }
    int index = (int)AS_NUMBER(pop(vm));

    Value value = pop(vm);
    if (IS_TUPLE(value)) {
        ObjTuple* tuple = AS_TUPLE(value);
        if (index > tuple->size - 1 || index < 0) {
            runtimeError(vm, "Tuple index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        push(vm, tuple->elements[index]);
    } else if (IS_STRING(value)) {
        ObjString* string = AS_STRING(value);
        if (index > string->length - 1 || index < 0) {
            runtimeError(vm, "String index out of range");
            return INTERPRET_RUNTIME_ERROR;
        }

        // Probably make this a function
        int len = 1;
        unsigned char* chars = (char*)malloc(len + 1);
        if (!chars) {
            runtimeError(vm, "Out of memory :(");
            return INTERNAL_SOFTWARE_ERROR;
        }
        strncpy(chars, string->chars + index, len);
        chars[len] = '\0';

        push(vm, OBJ_VAL(copyString(&vm->gc, &vm->strings, chars, len)));
        free(chars);

    } else {
        runtimeError(vm, "Object cannot be subscripted");
        return INTERPRET_RUNTIME_ERROR;
    }

    return INTERPRET_OK;
}

static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE()     (*frame->ip++)
#define READ_SHORT()    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())
// --- Ugly ---
#define BINARY_OP(vm, valueType, op) \
    do { \
        if(!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
            runtimeError(vm, "Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, valueType(a op b)); \
    } while (false)
#define SET_OP(vm, valueType, setFunction) \
    do { \
        if (!IS_SET(peek(vm, 0)) || !IS_SET(peek(vm, 1))) { \
            runtimeError(vm, "Operands must be sets"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        ObjSet* setB = AS_SET(pop(vm)); \
        ObjSet* setA = AS_SET(pop(vm)); \
        push(vm, valueType(setFunction(&vm->gc, setA, setB))); \
    } while (false)
// ---

    while(true) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for(Value* slot = vm->stack; slot < vm->stackTop; slot++) {
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
                push(vm, constant);
                break;
            }
            case OP_NULL: push(vm, NULL_VAL); break;
            case OP_TRUE: push(vm, BOOL_VAL(true)); break;
            case OP_FALSE: push(vm, BOOL_VAL(false)); break;
            case OP_POP: pop(vm); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm->globals, name, &value)) {
                    runtimeError(vm, "Undefined variable '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm->gc, &vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm->gc, &vm->globals, name, peek(vm, 0))) {
                    tableDelete(&vm->globals, name);
                    runtimeError(vm, "Undefined variable '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(vm, *frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(vm, 0);
                break;
            }
            case OP_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                
                if (IS_BOOL(b) || IS_BOOL(a)) {
                    // Use truth values
                    push(vm, BOOL_VAL(isFalse(b) == isFalse(a)));
                } else {
                    push(vm, BOOL_VAL(valuesEqual(a, b)));
                }

                break;
            }
            case OP_NOT_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(!valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(vm, BOOL_VAL, >); break;
            case OP_GREATER_EQUAL: BINARY_OP(vm, BOOL_VAL, >=); break;
            case OP_LESS: BINARY_OP(vm, BOOL_VAL, <); break;
            case OP_LESS_EQUAL: BINARY_OP(vm, BOOL_VAL, <=); break;
            case OP_ADD: {
                if (IS_STRING(peek(vm, 0)) || IS_STRING(peek(vm, 1))) {
                    // Concatenate if at least one operand is a string
                    Value b = pop(vm);
                    Value a = pop(vm);

                    ObjString* aStr = AS_STRING(IS_STRING(a) ? a : b);
                    unsigned char* bStr = valueToString(IS_STRING(a) ? b : a);

                    ObjString* result = concatStrings(&vm->gc, &vm->strings, aStr->chars, aStr->length, aStr->hash, bStr, strlen(bStr));
                    push(vm, OBJ_VAL(result));
                } else if (IS_TUPLE(peek(vm, 0)) && IS_TUPLE(peek(vm, 1))) {
                    ObjTuple* b = AS_TUPLE(pop(vm));
                    ObjTuple* a = AS_TUPLE(pop(vm));
                    push(vm, OBJ_VAL(concatenateTuple(&vm->gc, a, b)));
                } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    // Else, numerically add
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                } else {
                    runtimeError(vm, "Invalid operand type(s)");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(vm, NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(vm, NUMBER_VAL, *); break;
            case OP_MOD: {
                if (!IS_INTEGER(peek(vm, 0)) || !IS_INTEGER(peek(vm, 1))) {
                    runtimeError(vm, "Operands must be integers");
                    return INTERPRET_RUNTIME_ERROR;
                } else if (IS_NUMBER(peek(vm, 0)) && AS_NUMBER(peek(vm, 0)) == 0) {
                    // Return error if divisor is 0
                    runtimeError(vm, "Division by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }

                int b = AS_NUMBER(pop(vm));
                int a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a % b));
                break;
            }
            case OP_DIVIDE: {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
                    runtimeError(vm, "Operands must be numbers");
                    return INTERPRET_RUNTIME_ERROR;
                } else if (IS_NUMBER(peek(vm, 0)) && AS_NUMBER(peek(vm, 0)) == 0) {
                    // Return error if divisor is 0
                    runtimeError(vm, "Division by 0");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a / b));
                break;
            }
            case OP_EXPONENT: {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
                    runtimeError(vm, "Operands must be numbers");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(pow(a, b)));
                break;
            }
            case OP_NOT: {   
                push(vm, BOOL_VAL(isFalse(pop(vm)))); 
                break;
            }
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtimeError(vm, "Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalse(peek(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE_2: { // Pops the condition always. Couldn't think of anything better
                uint16_t offset = READ_SHORT();
                if (isFalse(peek(vm, 0))) {
                    frame->ip += offset;
                    pop(vm);
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
                if (!callValue(vm, peek(vm, argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(&vm->gc, function);
                push(vm, OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;
            }
            case OP_RETURN: {
                bool implicitReturn = READ_BYTE();
                Value result;
                if (implicitReturn) {
                    result = vm->impReturnStash;
                    vm->impReturnStash = NULL_VAL;
                } else {
                    result = pop(vm);
                }

                closeUpvalues(vm, frame->slots);
                vm->frameCount--;
                if (vm->frameCount == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }

                vm->stackTop = frame->slots;
                push(vm, result);
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_STASH: {
                vm->impReturnStash = pop(vm);
                break;
            }
            case OP_SET_CREATE: {
                ObjSet* set = newSet(&vm->gc);
                push(vm, OBJ_VAL(set));
                break;
            }
            case OP_SET_INSERT: {
                Value value = pop(vm);
                Value setVal = pop(vm);
                ObjSet* set = AS_SET(setVal);
                setInsert(&vm->gc, set, value);
                push(vm, setVal);
                break;
            }
            case OP_SET_INSERT_2: { // Inserts two values off the stack - not great and should probably be the same as OP_SET_INSERT
                Value valueA = pop(vm);
                Value valueB = pop(vm);
                Value setVal = pop(vm);
                ObjSet* set = AS_SET(setVal);
                setInsert(&vm->gc, set, valueB);
                setInsert(&vm->gc, set, valueA);
                push(vm, setVal);
                break;
            }
            case OP_SET_OMISSION: {
                bool omissionParameter = READ_BYTE();
                int status = setOmission(vm, omissionParameter);
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_SET_IN: {
                if (!IS_SET(peek(vm, 0))) {
                    runtimeError(vm, "Right hand operand must be a set");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSet* set = AS_SET(pop(vm));
                Value value = pop(vm);
                push(vm, BOOL_VAL(setContains(set, value)));
                break;
            }
            case OP_SET_INTERSECT: SET_OP(vm, OBJ_VAL, setIntersect); break;
            case OP_SET_UNION: SET_OP(vm, OBJ_VAL, setUnion); break;
            case OP_SET_DIFFERENCE: SET_OP(vm, OBJ_VAL, setDifference); break;
            case OP_SUBSET: SET_OP(vm, BOOL_VAL, isProperSubset); break;
            case OP_SUBSETEQ: SET_OP(vm, BOOL_VAL, isSubset); break;
            case OP_SIZE: {
                Value value = pop(vm);
                switch (value.type) {
                    case VAL_NUMBER:
                        push(vm, NUMBER_VAL(fabs(AS_NUMBER(value))));
                        break;
                    case VAL_OBJ:
                        switch (AS_OBJ(value)->type) {
                            case OBJ_STRING:
                                push(vm, NUMBER_VAL(AS_STRING(value)->length));
                                break;
                            case OBJ_SET:
                                push(vm, NUMBER_VAL(AS_SET(value)->count));
                                break;
                            case OBJ_TUPLE:
                                push(vm, NUMBER_VAL(AS_TUPLE(value)->size));
                                break;
                            default:
                                runtimeError(vm, "Invalid operand type");
                                return INTERPRET_RUNTIME_ERROR;
                        }
                        break;
                    case VAL_NULL:
                    case VAL_BOOL:
                        runtimeError(vm, "Invalid operand type");
                        return INTERPRET_RUNTIME_ERROR;
                    default: break;
                }
                break;
            }
            case OP_CREATE_TUPLE: {
                int arity = READ_BYTE();
                ObjTuple* tuple = newTuple(&vm->gc, arity);
                for (int i = arity - 1; i >= 0; i--) {
                    tuple->elements[i] = pop(vm);
                }
                push(vm, OBJ_VAL(tuple));
                break;
            }
            case OP_TUPLE_OMISSION: {
                bool omissionParameter = READ_BYTE();
                int status = tupleOmission(vm, omissionParameter);
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_SUBSCRIPT: {
                int status = subscript(vm);
                if (status != INTERPRET_OK) return status;
                break;
            }
            case OP_CREATE_ITERATOR: {
                if (!IS_SET(peek(vm, 0))) {
                    runtimeError(vm, "Generator must iterate over a set");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSet* set = AS_SET(pop(vm));
                ObjSetIterator* iterator = newSetIterator(&vm->gc, set);
                push(vm, OBJ_VAL(iterator));
                break;
            }
            case OP_ITERATE: {
                if (!IS_SET_ITERATOR(peek(vm, 0))) {
                    runtimeError(vm, "(Internal) Missing iterator");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSetIterator* iterator = AS_SET_ITERATOR(pop(vm));
                Value value;
                bool hasCurrentVal = iterateSetIterator(iterator, &value);
                if (hasCurrentVal) push(vm, value);
                push(vm, BOOL_VAL(hasCurrentVal));
                break;
            }
            case OP_ARB: {
                if (!IS_SET(peek(vm, 0))) {
                    runtimeError(vm, "Expected set after some keyword");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjSet* set = AS_SET(pop(vm));
                push(vm, getArb(set));
                break;
            }
            default: {
                runtimeError(vm, "(Internal) Invalid Opcode");
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

InterpretResult interpret(VM* vm, const unsigned char* source) {
    ObjFunction* function = compile(source, &vm->gc, &vm->strings);
    if(function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(vm, OBJ_VAL(function));
    
    ObjClosure* closure = newClosure(&vm->gc, function);
    pop(vm);
    push(vm, OBJ_VAL(closure));
    call(vm, closure, 0);

    return run(vm);
}