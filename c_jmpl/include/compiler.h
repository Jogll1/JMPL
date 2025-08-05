#ifndef c_jmpl_compiler_h
#define c_jmpl_compiler_h

#include "vm.h"
#include "scanner.h"
#include "gc.h"

ObjFunction* compile(GC* gc, const unsigned char* source);
void markCompilerRoots();

#endif