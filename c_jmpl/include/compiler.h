#ifndef c_jmpl_compiler_h
#define c_jmpl_compiler_h

#include <ctype.h>

#include "vm.h"

ObjFunction* compile(const unsigned char* source);
void markCompilerRoots();

#endif