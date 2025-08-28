#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "obj_string.h"
#include "vm.h"
#include "native.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#define JMPL_PI 3.14159265358979323846
#define JMPL_EPSILON 1e-10

#define LOAD_NATIVE(name) name##Native

/**
 * @brief Adds a native function.
 * 
 * @param name     The name of the function in JMPL
 * @param arity    How many parameters it should have
 * @param function The C function that is called
 */
static void defineNative(ObjModule* module, const unsigned char* name, int arity, NativeFn function) {
    ObjString* nameStr = copyString(&vm.gc, name, (int)strlen(name));
    pushTemp(&vm.gc, OBJ_VAL(nameStr));
    ObjNative* native = newNative(&vm.gc, function, arity);
    pushTemp(&vm.gc, OBJ_VAL(native));

    tableSet(&vm.gc, &module->globals, nameStr, OBJ_VAL(native));
    popTemp(&vm.gc);
    popTemp(&vm.gc); 
}

void loadModule(ObjModule* module) {
    tableAddAll(&vm.gc, &module->globals, &vm.globals);
}

// ==============================================================
// ===================== Core               =====================
// ==============================================================

// --- General purpose ---

/**
 * clock()
 * 
 * Returns how long the program has elapsed in seconds.
 */
DEF_NATIVE(clock) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

/**
 * sleep(x)
 * 
 * Sleeps for x seconds.
 */
DEF_NATIVE(sleep) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;

    double seconds = AS_NUMBER(args[0]);
    if (seconds < 0) return NULL_VAL;

#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000));
#else
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
#endif

    return NULL_VAL;
}

// --- I/O ---

/**
 * print(x)
 * 
 * Prints a value to the console.
 * Returns null.
 */
DEF_NATIVE(print) {
    printValue(args[0], false);

    return NULL_VAL;
}

/**
 * println(x)
 * 
 * Prints a value to the console with a newline.
 * Returns null.
 */
DEF_NATIVE(println) {
    printValue(args[0], false);
    printf("\n");

    return NULL_VAL;
}

/**
 * input()
 * 
 * Reads a line of input.
 */
DEF_NATIVE(input) {
    size_t size = 32;
    size_t len = 0;
    unsigned char* buffer = malloc(size);

    if (!buffer) {
        exit(INTERNAL_SOFTWARE_ERROR);
    }

    int c;
    while ((c = getchar()) != EOF && c != '\n') {
        buffer[len++] = c; 

        if (len == size) {
            size *= 2;
            char* temp = realloc(buffer, size);

            if (!temp) {
                free(buffer);
                exit(INTERNAL_SOFTWARE_ERROR);
            }

            buffer = temp;
        }
    }
    
    ObjString* str = copyString(&vm->gc, buffer, len);
    free(buffer);
    return OBJ_VAL(str);
}

// --- Types ---

/**
 * type(x)
 * 
 * Returns the type of a value as a string.
 */
DEF_NATIVE(type) {
    Value value = args[0];

    unsigned char* str;
    if (IS_BOOL(value)) {
        str = "BOOL";
    } else if (IS_NULL(value)) {
        str = "NULL";
    } else if (IS_CHAR(value)) {
        str = "CHAR";
    } else if (IS_NUMBER(value)) {
        str = "NUMBER";
    } else if (IS_OBJ(value)) {
        switch(AS_OBJ(value)->type) {
            case OBJ_FUNCTION: str = "FUNCTION"; break;
            case OBJ_CLOSURE:  str = "FUNCTION"; break;
            case OBJ_NATIVE:   str = "NATIVE";   break;
            case OBJ_SET:      str = "SET";      break;
            case OBJ_TUPLE:    str = "TUPLE";    break;
            case OBJ_STRING:   str = "STRING";   break;
            default:           str = "UNKNOWN";  break;
        }
    } else {
        str = "UNKNOWN";
    }

    return OBJ_VAL(copyString(&vm->gc, str, strlen(str)));
}

/**
 * toNum(x)
 * 
 * Casts a value to a number. Returns null for invalid types.
 * Valid types are bool, number, char, and string.
 */
DEF_NATIVE(toNum) {
    Value value = args[0];

    if (IS_BOOL(value)) {
        return NUMBER_VAL(AS_BOOL(value));
    } else if (IS_CHAR(value)) {
        return NUMBER_VAL(AS_CHAR(value));
    } else if (IS_NUMBER(value)) {
        return value;
    } else if (IS_STRING(value)) {
        ObjString* string = AS_STRING(value);
        double value = strtod(string->utf8, NULL);
        return NUMBER_VAL(value);
    } else {
        return NULL_VAL;
    }
}

/**
 * toStr(x)
 * 
 * Casts a value to an string. Works for all types.
 */
DEF_NATIVE(toStr) {
    Value value = args[0];
    unsigned char* str = valueToString(value);
    ObjString* string = copyString(&vm->gc, str, strlen(str));
    free(str);
    return OBJ_VAL(string);
}

/**
 * @brief Define the natives in the core library.
 */
ObjModule* defineCoreLibrary() {
    unsigned char* name = "core";
    ObjModule* core = newModule(&vm.gc, copyString(&vm.gc, name, strlen(name)));
    
    // General purpose
    defineNative(core, "clock", 0, LOAD_NATIVE(clock));
    defineNative(core, "sleep", 1, LOAD_NATIVE(sleep));

    // I/O
    defineNative(core, "print", 1, LOAD_NATIVE(print));
    defineNative(core, "println", 1, LOAD_NATIVE(println));

    defineNative(core, "input", 0, LOAD_NATIVE(input));

    // Types
    defineNative(core, "type", 1, LOAD_NATIVE(type));
    defineNative(core, "toNum", 1, LOAD_NATIVE(toNum));
    defineNative(core, "toStr", 1, LOAD_NATIVE(toStr));

    pushTemp(&vm.gc, OBJ_VAL(core));
    tableSet(&vm.gc, &vm.modules, core->name, OBJ_VAL(core));
    popTemp(&vm.gc);

    return core;
}

// ==============================================================
// ===================== Maths              =====================
// ==============================================================

/**
 * pi()
 * 
 * Returns the numerical value of pi.
 */
DEF_NATIVE(pi) {
    return NUMBER_VAL(JMPL_PI);
}

/**
 * sin(x)
 * 
 * Returns the sine of x (in radians).
 * Returns null if x is not a number.
 * Manually returns 0 for all +/- n * pi
 */
DEF_NATIVE(sin) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;

    double arg = AS_NUMBER(args[0]);

    if (fabs((arg / JMPL_PI) - round(arg / JMPL_PI)) < JMPL_EPSILON) {
        return NUMBER_VAL(0);
    }

    return NUMBER_VAL(sin(arg));
}

/**
 * cos(x)
 * 
 * Returns the cosine of x (in radians).
 * Returns null if x is not a number.
 * Manually returns 0 for all +/- (2n + 1) * pi / 2
 */
DEF_NATIVE(cos) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    double arg = AS_NUMBER(args[0]);

    if ((fabs((arg / (JMPL_PI / 2)) - round(arg / (JMPL_PI / 2))) < JMPL_EPSILON && fmod(round(arg / (JMPL_PI / 2)), 2) != 0)) {
        return NUMBER_VAL(0);
    }

    return NUMBER_VAL(cos(arg));
}

/**
 * tan(x)
 * 
 * Returns the tangent of x (in radians).
 * Returns null if x is not a number.
 * Manually returns 0 for all +/- n * pi
 */
DEF_NATIVE(tan) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    double arg = AS_NUMBER(args[0]);

    if (fabs((arg / JMPL_PI) - round(arg / JMPL_PI)) < JMPL_EPSILON) {
        return NUMBER_VAL(0);
    }
    else if ((fabs((arg / (JMPL_PI / 2)) - round(arg / (JMPL_PI / 2))) < JMPL_EPSILON && fmod(round(arg / (JMPL_PI / 2)), 2) != 0)) {
        return NULL_VAL;
    }

    return NUMBER_VAL(tan(arg));
}

/**
 * arcsin(x)
 * 
 * Returns the arcsine of x (in radians).
 * Returns null if x is not a number.
 */
DEF_NATIVE(arcsin) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(asin(AS_NUMBER(args[0])));
}

/**
 * arccos(x)
 * 
 * Returns the arccosine of x (in radians).
 * Returns null if x is not a number.
 */
DEF_NATIVE(arccos) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(acos(AS_NUMBER(args[0])));
}

/**
 * arctan(x)
 * 
 * Returns the arctangent of x (in radians).
 * Returns null if x is not a number.
 */
DEF_NATIVE(arctan) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(atan(AS_NUMBER(args[0])));
}

/**
 * max(x, y)
 * 
 * Returns the maximum of x and y.
 * Returns null if either x or y is not a number.
 */
DEF_NATIVE(max) {
    if(!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return NULL_VAL;
    
    double arg1 = AS_NUMBER(args[0]);
    double arg2 = AS_NUMBER(args[1]);

    return NUMBER_VAL(arg1 > arg2 ? arg1 : arg2);
}

/**
 * min(x, y)
 * 
 * Returns the minimum of x and y.
 * Returns null if either x or y is not a number.
 */
DEF_NATIVE(min) {
    if(!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return NULL_VAL;
    
    double arg1 = AS_NUMBER(args[0]);
    double arg2 = AS_NUMBER(args[1]);

    return NUMBER_VAL(arg1 < arg2 ? arg1 : arg2);
}

/**
 * floor(x)
 * 
 * Returns the floor of x.
 * Returns null if x is not a number.
 */
DEF_NATIVE(floor) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

/**
 * ceil(x)
 * 
 * Returns the ceiling of x.
 * Returns null if x is not a number.
 */
DEF_NATIVE(ceil) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

/**
 * round(x)
 * 
 * Returns the rounded value of x.
 * Returns null if x is not a number.
 */
DEF_NATIVE(round) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

/**
 * @brief Define the natives in the maths library.
 */
ObjModule* defineMathLibrary() {
    unsigned char* name = "math";
    ObjModule* math = newModule(&vm.gc, copyString(&vm.gc, name, strlen(name)));

    defineNative(math, "pi", 0, LOAD_NATIVE(pi));

    defineNative(math, "sin", 1, LOAD_NATIVE(sin));
    defineNative(math, "cos", 1, LOAD_NATIVE(cos));
    defineNative(math, "tan", 1, LOAD_NATIVE(tan));
    defineNative(math, "arcsin", 1, LOAD_NATIVE(arcsin));
    defineNative(math, "arccos", 1, LOAD_NATIVE(arccos));
    defineNative(math, "arctan", 1, LOAD_NATIVE(arctan));

    defineNative(math, "max", 2, LOAD_NATIVE(max));
    defineNative(math, "min", 2, LOAD_NATIVE(min));
    defineNative(math, "floor", 1, LOAD_NATIVE(floor));
    defineNative(math, "ceil", 1, LOAD_NATIVE(ceil));
    defineNative(math, "round", 1, LOAD_NATIVE(round));

    pushTemp(&vm.gc, OBJ_VAL(math));
    tableSet(&vm.gc, &vm.modules, math->name, OBJ_VAL(math));
    popTemp(&vm.gc);

    return math;
}