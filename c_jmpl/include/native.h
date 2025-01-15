#ifndef c_jmpl_native_h
#define c_jmpl_native_h

#include "common.h"
#include "value.h"

// NATIVE FUNCTIONS

// General purpose
/**
 * clock()
 * 
 * Returns how long the program has elapsed in seconds.
 */
Value clockNative(int argCount, Value* args);

// Maths library (built-in by design)
/**
 * sin(x)
 * 
 * Returns the sine of x (in radians).
 * Returns null if x is not a number.
 */
Value sinNative(int argCount, Value* args);
/**
 * cos(x)
 * 
 * Returns the cosine of x (in radians).
 * Returns null if x is not a number.
 */
Value cosNative(int argCount, Value* args);
/**
 * tan(x)
 * 
 * Returns the tangent of x (in radians).
 * Returns null if x is not a number.
 */
Value tanNative(int argCount, Value* args);
/**
 * arcsin(x)
 * 
 * Returns the arcsine of x (in radians).
 * Returns null if x is not a number.
 */
Value arcsinNative(int argCount, Value* args);
/**
 * arccos(x)
 * 
 * Returns the arccosine of x (in radians).
 * Returns null if x is not a number.
 */
Value arccosNative(int argCount, Value* args);
/**
 * arctan(x)
 * 
 * Returns the arctangent of x (in radians).
 * Returns null if x is not a number.
 */
Value arctanNative(int argCount, Value* args);

#endif