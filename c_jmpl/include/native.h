#ifndef c_jmpl_native_h
#define c_jmpl_native_h

#include "common.h"
#include "value.h"

// Native functions
/**
 * Returns how long the program has elapsed in seconds.
 */
Value clockNative(int argCount, Value* args);

#endif