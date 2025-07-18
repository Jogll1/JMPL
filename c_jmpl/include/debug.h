#ifndef c_jmpl_debug_h
#define c_jmpl_debug_h

#include <stdio.h>
#include <stdarg.h>

#include "chunk.h"
#include "scanner.h"

typedef struct {
    bool printCode;
    bool traceExecution;
    bool printTokens;

    bool stressGC;
    bool logGC;

    FILE* logFile;
    bool logToFile;
} Debugger;

const unsigned char* getTokenName(TokenType type);

void initDebugger(Debugger* debugger);
void logMessage(Debugger* debugger, bool logToConsole, const char* format, ...);

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif