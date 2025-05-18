#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "native.h"

// ------------ General purpose ------------

/**
 * clock()
 * 
 * Returns how long the program has elapsed in seconds.
 */
Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// ------------ Maths library ------------

/**
 * sin(x)
 * 
 * Returns the sine of x (in radians).
 * Returns null if x is not a number.
 */
Value sinNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

/**
 * cos(x)
 * 
 * Returns the cosine of x (in radians).
 * Returns null if x is not a number.
 */
Value cosNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

/**
 * tan(x)
 * 
 * Returns the tangent of x (in radians).
 * Returns null if x is not a number.
 */
Value tanNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(tan(AS_NUMBER(args[0])));
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