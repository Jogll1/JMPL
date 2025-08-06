#ifndef c_jmpl_native_h
#define c_jmpl_native_h

#include "common.h"
#include "value.h"
#include "vm.h"

// NATIVE FUNCTIONS

// --- General purpose ---

Value clockNative(VM* vm, int argCount, Value* args);
Value sleepNative(VM* vm, int argCount, Value* args);

Value typeNative(VM* vm, int argCount, Value* args);

// --- I/O ---

Value printNative(VM* vm, int argCount, Value* args);
Value printlnNative(VM* vm, int argCount, Value* args);

Value inputNative(VM* vm, int argCount, Value* args);

// --- Maths library (built-in by design) ---

Value piNative(VM* vm, int argCount, Value* args);

Value sinNative(VM* vm, int argCount, Value* args);
Value cosNative(VM* vm, int argCount, Value* args);
Value tanNative(VM* vm, int argCount, Value* args);
Value arcsinNative(VM* vm, int argCount, Value* args);
Value arccosNative(VM* vm, int argCount, Value* args);
Value arctanNative(VM* vm, int argCount, Value* args);

Value maxNative(VM* vm, int argCount, Value* args);
Value minNative(VM* vm, int argCount, Value* args);
Value floorNative(VM* vm, int argCount, Value* args);
Value ceilNative(VM* vm, int argCount, Value* args);
Value roundNative(VM* vm, int argCount, Value* args);

#endif