#ifndef c_jmpl_debug_h
#define c_jmpl_debug_h

#include <stdio.h>
#include <stdarg.h>

#include "chunk.h"
#include "scanner.h"

const unsigned char* getTokenName(TokenType type);

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif