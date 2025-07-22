#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "native.h"

#define JMPL_PI 3.14159265358979323846
#define JMPL_EPSILON 1e-10

// --- General purpose ---

/**
 * clock()
 * 
 * Returns how long the program has elapsed in seconds.
 */
Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// --- I/O ---

/**
 * print(x)
 * 
 * Prints a value to the console.
 * Returns null.
 */
Value printNative(int argCount, Value* args) {
    printValue(args[0]);

    return NULL_VAL;
}

/**
 * println(x)
 * 
 * Prints a value to the console with a newline.
 * Returns null.
 */
Value printlnNative(int argCount, Value* args) {
    printValue(args[0]);
    printf("\n");

    return NULL_VAL;
}

// --- Maths library ---

/**
 * pi()
 * 
 * Returns the numerical value of pi.
 */
Value piNative(int argCount, Value* args) {
    return NUMBER_VAL(JMPL_PI);
}

/**
 * sin(x)
 * 
 * Returns the sine of x (in radians).
 * Returns null if x is not a number.
 * Manually returns 0 for all +/- n * pi
 */
Value sinNative(int argCount, Value* args) {
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
Value cosNative(int argCount, Value* args) {
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
Value tanNative(int argCount, Value* args) {
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
Value arcsinNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(asin(AS_NUMBER(args[0])));
}

/**
 * arccos(x)
 * 
 * Returns the arccosine of x (in radians).
 * Returns null if x is not a number.
 */
Value arccosNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(acos(AS_NUMBER(args[0])));
}

/**
 * arctan(x)
 * 
 * Returns the arctangent of x (in radians).
 * Returns null if x is not a number.
 */
Value arctanNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(atan(AS_NUMBER(args[0])));
}

/**
 * max(x, y)
 * 
 * Returns the maximum of x and y.
 * Returns null if either x or y is not a number.
 */
Value maxNative(int argCount, Value* args) {
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
Value minNative(int argCount, Value* args) {
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
Value floorNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

/**
 * ceil(x)
 * 
 * Returns the ceiling of x.
 * Returns null if x is not a number.
 */
Value ceilNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

/**
 * round(x)
 * 
 * Returns the rounded value of x.
 * Returns null if x is not a number.
 */
Value roundNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL;
    
    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}