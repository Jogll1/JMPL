#ifndef c_jmpl_native_h
#define c_jmpl_native_h

#include "common.h"
#include "value.h"

// NATIVE FUNCTIONS

// ------------ General purpose ------------

Value clockNative(int argCount, Value* args);

// ------------ Maths library (built-in by design) ------------

Value sinNative(int argCount, Value* args);
Value cosNative(int argCount, Value* args);
Value tanNative(int argCount, Value* args);
Value arcsinNative(int argCount, Value* args);
Value arccosNative(int argCount, Value* args);
Value arctanNative(int argCount, Value* args);

#endif