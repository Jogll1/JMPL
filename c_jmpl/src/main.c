#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"

//gcc -I.\c_jmpl\include -o .\build\c_jmpl\main .\c_jmpl\src\*.c
//.\build\c_jmpl\main.exe

int main(int argc, const char* agv[]) {
    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);
    return 0;
}