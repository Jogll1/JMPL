#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "native.h"

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

#define JMPL_PI 3.14159265358979323846
#define JMPL_EPSILON 1e-10

// --- General purpose ---

/**
 * clock()
 * 
 * Returns how long the program has elapsed in seconds.
 */
Value clockNative(VM* vm, int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

/**
 * sleep(x)
 * 
 * Sleeps for x seconds.
 */
Value sleepNative(VM* vm, int argCount, Value* args) {
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

/**
 * type(x)
 * 
 * Returns the type of a value as a string.
 */
Value typeNative(VM* vm, int argCount, Value* args) {
    Value value = args[0];

    unsigned char* str;
    if (IS_BOOL(value)) {
        str = "bool";
    } else if (IS_NULL(value)) {
        str = "null";
    } else if (IS_NUMBER(value)) {
        str = "number";
    } else if (IS_OBJ(value)) {
        switch(AS_OBJ(value)->type) {
            case OBJ_FUNCTION: str = "function"; break;
            case OBJ_CLOSURE:  str = "function"; break;
            case OBJ_NATIVE:   str = "native";   break;
            case OBJ_SET:      str = "set";      break;
            case OBJ_TUPLE:    str = "tuple";    break;
            case OBJ_STRING:   str = "string";   break;
            default:           str = "unknown";  break;
        }
    } else {
        str = "unknown";
    }

    return OBJ_VAL(copyString(&vm->gc, str, strlen(str)));
}

// --- I/O ---

/**
 * print(x)
 * 
 * Prints a value to the console.
 * Returns null.
 */
Value printNative(VM* vm, int argCount, Value* args) {
    printValue(args[0], false);

    return NULL_VAL;
}

/**
 * println(x)
 * 
 * Prints a value to the console with a newline.
 * Returns null.
 */
Value printlnNative(VM* vm, int argCount, Value* args) {
    printValue(args[0], false);
    printf("\n");

    return NULL_VAL;
}

/**
 * input()
 * 
 * Reads a line of input.
 */
Value inputNative(VM* vm, int argCount, Value* args) {
    size_t size = 32;
    size_t len = 0;
    unsigned char* buffer = malloc(size);

    if (!buffer) {
        exit(INTERNAL_SOFTWARE_ERROR);
    }

    int c;
    while((c = getchar()) != EOF && c != '\n') {
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

// --- Maths library ---

/**
 * pi()
 * 
 * Returns the numerical value of pi.
 */
Value piNative(VM* vm, int argCount, Value* args) {
    return NUMBER_VAL(JMPL_PI);
}

/**
 * sin(x)
 * 
 * Returns the sine of x (in radians).
 * Returns null if x is not a number.
 * Manually returns 0 for all +/- n * pi
 */
Value sinNative(VM* vm, int argCount, Value* args) {
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
Value cosNative(VM* vm, int argCount, Value* args) {
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
Value tanNative(VM* vm, int argCount, Value* args) {
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
Value arcsinNative(VM* vm, int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(asin(AS_NUMBER(args[0])));
}

/**
 * arccos(x)
 * 
 * Returns the arccosine of x (in radians).
 * Returns null if x is not a number.
 */
Value arccosNative(VM* vm, int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(acos(AS_NUMBER(args[0])));
}

/**
 * arctan(x)
 * 
 * Returns the arctangent of x (in radians).
 * Returns null if x is not a number.
 */
Value arctanNative(VM* vm, int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(atan(AS_NUMBER(args[0])));
}

/**
 * max(x, y)
 * 
 * Returns the maximum of x and y.
 * Returns null if either x or y is not a number.
 */
Value maxNative(VM* vm, int argCount, Value* args) {
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
Value minNative(VM* vm, int argCount, Value* args) {
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
Value floorNative(VM* vm, int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

/**
 * ceil(x)
 * 
 * Returns the ceiling of x.
 * Returns null if x is not a number.
 */
Value ceilNative(VM* vm, int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

/**
 * round(x)
 * 
 * Returns the rounded value of x.
 * Returns null if x is not a number.
 */
Value roundNative(VM* vm, int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}