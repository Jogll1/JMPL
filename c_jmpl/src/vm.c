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

// Check for types on the stack
#define T_BOOL(n)     (IS_BOOL(peek(n)))
#define T_NULL(n)     (IS_NULL(peek(n)))
#define T_NUM(n)      (IS_NUMBER(peek(n)))
#define T_INT(n)      (IS_INTEGER(peek(n)))
#define T_CHAR(n)     (IS_CHAR(peek(n)))
#define T_OBJ(n)      (IS_OBJ(peek(n)))

#define T_CLOSURE(n)  (IS_CLOSURE(peek(n)))
#define T_FUNC(n)     (IS_FUNCTION(peek(n)))
#define T_NATIVE(n)   (IS_NATIVE(peek(n)))
#define T_STRING(n)   (IS_STRING(peek(n)))
#define T_MODULE(n)   (IS_MODULE(peek(n)))
#define T_SET(n)      (IS_SET(peek(n)))
#define T_TUPLE(n)    (IS_TUPLE(peek(n)))
#define T_ITER(n)     (IS_ITERATOR(peek(n)))

// Check for runtime errors - if !condition then return error
#define ASSERT_THAT(condition, message) \
    do { \
        if (!(condition)) { \
            runtimeError(message); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
    } while(false)


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

static inline void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

static inline Value pop() {
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

    if (vm.frameCount == FRAMES_MAX) {
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
    bool isIntOmission = T_INT(0) && T_INT(1) && (!hasNext || T_INT(2));
    bool isCharOmission = T_CHAR(0) && T_CHAR(1) && (!hasNext || T_CHAR(2));

    ASSERT_THAT(isIntOmission || isCharOmission, "Terms of an omission operation must be all integers or all bcharacters");

    int last = (int)(isCharOmission ? AS_CHAR(pop()) : AS_NUMBER(pop()));
    int next = 0;
    if (hasNext) {
        next =  (int)(isCharOmission ? AS_CHAR(pop()) : AS_NUMBER(pop()));
    }
    int first =  (int)(isCharOmission ? AS_CHAR(pop()) : AS_NUMBER(pop()));

    int gap = hasNext ? abs(next - first) : 1;
    ASSERT_THAT(gap != 0, "Omission gap cannot be zero");

    int size = (int)floorl((double)abs(first - last) / (double)gap) + 1;

    int current = first;
    int step = first < last ? gap : -gap;

    if (isSet) {
        ObjSet* set = AS_SET(pop());
        pushTemp(&vm.gc, OBJ_VAL(set));

        for (int i = 0; i < size; i++) {
            setInsert(&vm.gc, set, isCharOmission ? CHAR_VAL(current) : NUMBER_VAL(current));
            current += step;
        }

        popTemp(&vm.gc); // set
        push(OBJ_VAL(set));
    } else {
        ObjTuple* tuple = newTuple(&vm.gc, size);

        for (int i = 0; i < size; i++) {
            tuple->elements[i] = isCharOmission ? CHAR_VAL(current) : NUMBER_VAL(current);
            current += step;
        }
        push(OBJ_VAL(tuple));
    }

    return INTERPRET_OK;
}

/**
 * @brief Determines if a value is falsey.
 * 
 * @param value The value to determine the boolean value of
 * 
 * Values are false if they are null, false, 0, or an empty object.
 */
static bool isFalse(Value value) {
    // Return false if null, false, 0, or empty object
    return IS_NULL(value) || 
           (IS_NUMBER(value) && AS_NUMBER(value) == 0) || 
           (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_STRING(value) && AS_CSTRING(value)[0] == '\0') ||
           (IS_SET(value) && AS_SET(value)->count == 0) ||
           (IS_TUPLE(value) && AS_TUPLE(value)->size == 0);
}

static int getSize(Value value) {
#ifdef JMPL_NAN_BOXING
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
    ASSERT_THAT(T_INT(0), "Index must be an integer");

    int index = (signed int)AS_NUMBER(pop());
    int length;

    Value value = pop();
    if (IS_TUPLE(value)) {
        length = (int)AS_TUPLE(value)->size;
    } else if (IS_STRING(value)) {
        length = (int)AS_STRING(value)->length;
    } else {
        ASSERT_THAT(false, "Object cannot be indexed");
    }

    ASSERT_THAT(index >= -length && index < length, "Index out of range");

    push(IS_TUPLE(value) ? indexTuple(AS_TUPLE(value), index) : indexString(AS_STRING(value), index));

    return INTERPRET_OK;
}

static InterpretResult sliceObj() {
    ASSERT_THAT((T_INT(0) || T_NULL(0)) && (T_INT(1) || T_NULL(1)), "Slice indices must be integers or null");

    Value end = pop();
    Value start = pop();

    int startIndex = IS_NULL(start) ? 0 : (signed int)AS_NUMBER(start);
    int endIndex;
    int length;

    Value value = pop();
    if (IS_TUPLE(value)) {
        length = (int)AS_TUPLE(value)->size;
    } else if (IS_STRING(value)) {
        length = (int)AS_STRING(value)->length;
    } else {
        ASSERT_THAT(false, "Object cannot be sliced");
    }

    endIndex = IS_NULL(end) ? length - 1 : (signed int)AS_NUMBER(end);
    ASSERT_THAT(startIndex >= -length && endIndex >= -length, "Slice index out of range");

    push(
        IS_TUPLE(value) ?
        OBJ_VAL(sliceTuple(&vm.gc, AS_TUPLE(value), startIndex, endIndex)) :
        OBJ_VAL(sliceString(&vm.gc, AS_STRING(value), startIndex, endIndex))
    );

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
        } else if (strcmp(path->utf8, "random") == 0) {
            return OBJ_VAL(defineRandomLibrary());
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
    if (function == NULL) {
        runtimeError("Could not compile module");
        return NULL_VAL;
    }

    function->name = moduleName;
    free(moduleSource);

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
#define BINARY_OP(op) \
    do { \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(NUMBER_VAL(a op b)); \
    } while (false)
#define ORDER_OP(op) \
    do { \
        ASSERT_THAT((T_NUM(0) || T_CHAR(0)) && (T_NUM(1) || T_CHAR(1)), "Operands must be numbers or characters"); \
        Value vb = pop(); \
        Value va = pop(); \
        double b = IS_CHAR(vb) ? AS_NUMBER(NUMBER_VAL(AS_CHAR(vb))) : AS_NUMBER(vb); \
        double a = IS_CHAR(va) ? AS_NUMBER(NUMBER_VAL(AS_CHAR(va))) : AS_NUMBER(va); \
        push(BOOL_VAL(a op b)); \
    } while (false)

#define SET_OP_GC(valueType, setFunction) \
    do { \
        ASSERT_THAT(T_SET(0) && T_SET(1), "Operands must be sets"); \
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
        ASSERT_THAT(T_SET(0) && T_SET(1), "Operands must be sets"); \
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

#ifdef JMPL_COMPUTED_GOTOS
    static void* dispatchTable[] = {
        #define OPCODE(name) &&op_##name,
        #include "opcodes.h"
        #undef OPCODE
    };
    
    #define INTERPRET_LOOP() DISPATCH();
    #define CASE_CODE(name)  op_##name

    #define DISPATCH() \
        do { \
            TRACE_EXECUTION(); \
            goto *dispatchTable[instruction = (OpCode)READ_BYTE()]; \
        } while (false)
#else
    #define INTERPRET_LOOP() \
        loop: \
            TRACE_EXECUTION(); \
            switch (instruction = (OpCode)READ_BYTE())

    #define CASE_CODE(name) case OP_##name
    #define DISPATCH()      goto loop
#endif

    LOAD_FRAME();

    OpCode instruction;
    INTERPRET_LOOP() {
        CASE_CODE(POP): pop(); DISPATCH();
        CASE_CODE(CONSTANT): {
            Value constant = READ_CONSTANT();
            push(constant);
            DISPATCH();
        }
        CASE_CODE(NULL): push(NULL_VAL); DISPATCH();
        CASE_CODE(TRUE): push(BOOL_VAL(true)); DISPATCH();
        CASE_CODE(FALSE): push(BOOL_VAL(false)); DISPATCH();
        CASE_CODE(GET_LOCAL): {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            DISPATCH();
        }
        CASE_CODE(SET_LOCAL): {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            DISPATCH();
        }
        CASE_CODE(GET_GLOBAL): {
            ObjString* name = READ_STRING();
            Value value;
            if (!tableGet(&vm.globals, name, &value)) {
                runtimeError("Undefined variable '%s'", name->utf8);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            DISPATCH();
        }
        CASE_CODE(DEFINE_GLOBAL): {
            ObjString* name = READ_STRING();
            tableSet(&vm.gc, &vm.globals, name, peek(0));
            pop();
            DISPATCH();
        }
        CASE_CODE(SET_GLOBAL): {
            ObjString* name = READ_STRING();
            if (tableSet(&vm.gc, &vm.globals, name, peek(0))) {
                tableDelete(&vm.globals, name);
                runtimeError("Undefined variable '%s'", name->utf8);
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(GET_UPVALUE): {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            DISPATCH();
        }
        CASE_CODE(SET_UPVALUE): {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            DISPATCH();
        }
        CASE_CODE(EQUAL): {
            Value b = pop();
            Value a = pop();
            
            if (IS_BOOL(b) || IS_BOOL(a)) {
                // Use truth values
                push(BOOL_VAL(isFalse(b) == isFalse(a)));
            } else {
                push(BOOL_VAL(valuesEqual(a, b)));
            }

            DISPATCH();
        }
        CASE_CODE(NOT_EQUAL): {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(!valuesEqual(a, b)));
            DISPATCH();
        }
        CASE_CODE(GREATER): ORDER_OP(>); DISPATCH();
        CASE_CODE(GREATER_EQUAL): ORDER_OP(>=); DISPATCH();
        CASE_CODE(LESS): ORDER_OP(<); DISPATCH();
        CASE_CODE(LESS_EQUAL): ORDER_OP(<=); DISPATCH();
        CASE_CODE(ADD): {
            if (T_STRING(0) || T_STRING(1)) {
                // Concatenate if at least one operand is a string
                Value b = pop();
                Value a = pop();
                push(OBJ_VAL(concatenateStringsHelper(&vm.gc, a, b)));
            } else if (T_TUPLE(0) && T_TUPLE(1)) {
                ObjTuple* b = AS_TUPLE(pop());
                ObjTuple* a = AS_TUPLE(pop());
                push(OBJ_VAL(concatenateTuple(&vm.gc, a, b)));
            } else if (T_NUM(0) && T_NUM(1)) {
                // Else, numerically add
                BINARY_OP(+);
            } else {
                ASSERT_THAT(false, "Invalid operand type(s)");
            }

            DISPATCH();
        }
        CASE_CODE(SUBTRACT): {
            ASSERT_THAT(T_NUM(0) && T_NUM(1), "Operands must be numbers");

            BINARY_OP(-);
            DISPATCH();
        }
        CASE_CODE(MULTIPLY): {
            ASSERT_THAT(T_NUM(0) && T_NUM(1), "Operands must be numbers");

            BINARY_OP(*);
            DISPATCH();
        }
        CASE_CODE(MOD): {
            ASSERT_THAT(T_INT(0) && T_INT(0), "Operands must be integers");
            ASSERT_THAT(AS_NUMBER(peek(0)) != 0, "Division by 0");

            int b = (int)AS_NUMBER(pop());
            int a = (int)AS_NUMBER(pop());
            push(NUMBER_VAL(a % b));
            DISPATCH();
        }
        CASE_CODE(DIVIDE): {
            ASSERT_THAT(T_NUM(0) && T_NUM(0), "Operands must be numbers");
            ASSERT_THAT(AS_NUMBER(peek(0)) != 0, "Division by 0");

            BINARY_OP(/);
            DISPATCH();
        }
        CASE_CODE(EXPONENT): {
            ASSERT_THAT(T_NUM(0) && T_NUM(1), "Operands must be numbers");

            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(pow(a, b)));
            DISPATCH();
        }
        CASE_CODE(NOT): {   
            push(BOOL_VAL(isFalse(pop()))); 
            DISPATCH();
        }
        CASE_CODE(NEGATE): {
            ASSERT_THAT(T_NUM(0), "Operand must be a number");

            push(NUMBER_VAL(-AS_NUMBER(pop())));
            DISPATCH();
        }
        CASE_CODE(JUMP): {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            DISPATCH();
        }
        CASE_CODE(JUMP_IF_FALSE): {
            uint16_t offset = READ_SHORT();
            if (isFalse(peek(0))) frame->ip += offset;
            DISPATCH();
        }
        CASE_CODE(JUMP_IF_FALSE_2): { // Pops the condition always. Couldn't think of anything better
            uint16_t offset = READ_SHORT();
            if (isFalse(peek(0))) {
                frame->ip += offset;
                pop();
            }
            DISPATCH();
        }
        CASE_CODE(LOOP): {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            DISPATCH();
        }
        CASE_CODE(CALL): {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            LOAD_FRAME();
            DISPATCH();
        }
        CASE_CODE(CLOSURE): {
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
            DISPATCH();
        }
        CASE_CODE(CLOSE_UPVALUE): {
            closeUpvalues(vm.stackTop - 1);
            pop();
            DISPATCH();
        }
        CASE_CODE(RETURN): {
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
            DISPATCH();
        }
        CASE_CODE(STASH): {
            vm.impReturnStash = pop();
            DISPATCH();
        }
        CASE_CODE(SET_CREATE): {
            ObjSet* set = newSet(&vm.gc);
            push(OBJ_VAL(set));
            DISPATCH();
        }
        CASE_CODE(SET_INSERT): {
            uint8_t count = READ_BYTE();
            assert(count > 0);
            setInsertN(count);
            DISPATCH();
        }
        CASE_CODE(SET_OMISSION): {
            bool omissionParameter = READ_BYTE();
            InterpretResult status = omission(true, omissionParameter);
            if (status != INTERPRET_OK) return status;
            DISPATCH();
        }
        CASE_CODE(SET_IN): {
            ASSERT_THAT(T_SET(0), "Right hand operand must be a set");

            ObjSet* set = AS_SET(pop());
            Value value = pop();
            push(BOOL_VAL(setContains(set, value)));
            DISPATCH();
        }
        CASE_CODE(SET_INTERSECT): SET_OP_GC(OBJ_VAL, setIntersect); DISPATCH();
        CASE_CODE(SET_UNION): SET_OP_GC(OBJ_VAL, setUnion); DISPATCH();
        CASE_CODE(SET_DIFFERENCE): SET_OP_GC(OBJ_VAL, setDifference); DISPATCH();
        CASE_CODE(SUBSET): SET_OP(BOOL_VAL, isProperSubset); DISPATCH();
        CASE_CODE(SUBSETEQ): SET_OP(BOOL_VAL, isSubset); DISPATCH();
        CASE_CODE(SIZE): {
            int size = getSize(pop());
            ASSERT_THAT(size != -1, "Invalid operand type");

            push(NUMBER_VAL(size));
            DISPATCH();
        }
        CASE_CODE(CREATE_TUPLE): {
            int arity = READ_BYTE();
            ObjTuple* tuple = newTuple(&vm.gc, arity);
            for (int i = arity - 1; i >= 0; i--) {
                tuple->elements[i] = pop();
            }
            push(OBJ_VAL(tuple));
            DISPATCH();
        }
        CASE_CODE(TUPLE_OMISSION): {
            bool omissionParameter = READ_BYTE();
            InterpretResult status = omission(false, omissionParameter);
            if (status != INTERPRET_OK) return status;
            DISPATCH();
        }
        CASE_CODE(SUBSCRIPT): {
            bool isSlice = READ_BYTE();
            InterpretResult status;
            if (isSlice) {
                status = sliceObj();
            } else {
                status = indexObj();
            }
            if (status != INTERPRET_OK) return status;
            DISPATCH();
        }
        CASE_CODE(CREATE_ITERATOR): {
            ASSERT_THAT(T_OBJ(0) && AS_OBJ(peek(0))->isIterable, "Generator must iterate over a set, tuple, or a string");

            Obj* target = AS_OBJ(pop());
            ObjIterator* iterator = newIterator(&vm.gc, target);
            push(OBJ_VAL(iterator));
            DISPATCH();
        }
        CASE_CODE(ITERATE): {
            ASSERT_THAT(T_ITER(0), "(Internal) Missing iterator");

            ObjIterator* iterator = AS_ITERATOR(pop());
            Value value;
            bool hasCurrentVal = iterateObj(iterator, &value);
            if (hasCurrentVal) push(value);
            push(BOOL_VAL(hasCurrentVal));
            DISPATCH();
        }
        CASE_CODE(ARB): {
            ASSERT_THAT(T_SET(0), "Expected set after arb keyword");

            ObjSet* set = AS_SET(pop());
            push(getArb(set));
            DISPATCH();
        }
        CASE_CODE(IMPORT_LIB): {
            ObjString* path = READ_STRING();
            push(importModule(path));

            if (T_CLOSURE(0)) {
                ObjClosure* closure = AS_CLOSURE(pop());
                call(closure, 0);
                LOAD_FRAME();
            } else if (T_MODULE(0)) {
                // Built-in or cached
                ObjModule* module = AS_MODULE(pop());
                loadModule(module);
            } else {
                // Resolve error
                pop();
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
    }

    ASSERT_THAT(false, "(Internal) Invalid Opcode");
#undef BINARY_OP
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