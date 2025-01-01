#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#define CURRENT_VERSION "v0.0.5"

// gcc -I.\c_jmpl\include -o .\build\c_jmpl\v0-0-5 .\c_jmpl\src\*.c
// .\build\c_jmpl\v0-0-5.exe

// ToDo:
// - RLE for line information in writeChunk (CH 14)
// - Constant long instruction (CH 14)
// - Dynamically grow stack (CH 15)
// - Remove repl line count
// - Add error types
// - Optimise storing global variable names as constants (CH 21)
// - POPN instruction that pops n amount of times (CH 22)
// - Constant variables, compiler error if try to reassign them (CH 22)
// - Remove local variable limit (CH 22), maybe dynamic arrays
// - Report runtime errors from native functions (CH 24)
//
// - format in compliance with the C style guide
// - add documentation

static void repl() {
    char line[1024];

    while(true) {
        printf("> ");

        if(!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static char* readFile(const char* path) {
    // Open file
    FILE* file = fopen(path, "rb");

    if(file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74); // I/O error
    }

    // Seek to the end and get how many bytes the file is
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    // Rewind file to the end
    rewind(file);

    // Allocate a string the size of the file
    unsigned char* buffer = (unsigned char*)malloc(fileSize + 1);

    if(buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74); // I/O error
    }

    // Read the whole file
    size_t bytesRead = fread(buffer, sizeof(unsigned char), fileSize, file);

    if(bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74); // I/O error
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const unsigned char* path) {
    unsigned char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if(result == INTERPRET_COMPILE_ERROR) exit(65); // Data format error
    if(result == INTERPRET_RUNTIME_ERROR) exit(70); // Internal software error
}

int main(int argc, const char* argv[]) {
    initVM();

    if(argc == 1) {
        // If no file argument, run the REPL
        printf("Welcome to the JMPL %s REPL.\n", CURRENT_VERSION);
        printf("Note: if using Windows, terminal must be using code page 65001 to use mathematical symbols.\n");
        repl();
    } else if(argc == 2) {
        // If there's a file argument, run the file
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: c_jmpl [path]\n");
        exit(64); // Command line usage error
    }

    freeVM();
    return 0;
}