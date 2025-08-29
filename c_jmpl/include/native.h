#ifndef c_jmpl_native_h
#define c_jmpl_native_h

#include "value.h"

#define DEF_NATIVE(name) Value name##Native(VM* vm, int argCount, Value* args)

void loadModule(ObjModule* module);

// ==============================================================
// ===================== Core              =====================
// ==============================================================

// --- General purpose ---

DEF_NATIVE(clock);
DEF_NATIVE(sleep);

// --- I/O ---

DEF_NATIVE(print);
DEF_NATIVE(println);

DEF_NATIVE(input);

// --- Types ---

DEF_NATIVE(type);
DEF_NATIVE(toNum);
DEF_NATIVE(toStr);
DEF_NATIVE(toChar);

ObjModule* defineCoreLibrary();

// ==============================================================
// ===================== Maths              =====================
// ==============================================================

// --- Constants --- 

DEF_NATIVE(pi);
DEF_NATIVE(e);
DEF_NATIVE(epsilon);

// --- Trigonometry --- 

DEF_NATIVE(sin);
DEF_NATIVE(cos);
DEF_NATIVE(tan);
DEF_NATIVE(arcsin);
DEF_NATIVE(arccos);
DEF_NATIVE(arctan);

// --- Misc. --- 

DEF_NATIVE(max);
DEF_NATIVE(min);
DEF_NATIVE(floor);
DEF_NATIVE(ceil);
DEF_NATIVE(round);

ObjModule* defineMathLibrary();

#endif