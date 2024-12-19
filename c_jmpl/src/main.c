#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

//gcc -I.\c_jmpl\include -o .\build\c_jmpl\main .\c_jmpl\src\*.c
//.\build\c_jmpl\main.exe

// ToDo:
// - RLE for line information in writeChunk (CH 14)
// - constant long instruction (CH 14)
// - Dynamically grow stack (CH 15)
// - Remove repl line count
// - Add error types
//
// - format in compliance with the C style guide
// - add documentation

// /**
//  * Read the next UTF-8 character of a file and adds it to buffer.
//  * 
//  * @param file   the file to read
//  * @param buffer where the character is to be stored
//  * @returns      the length of the character in bytes
//  */
// static int readUtf8Char(FILE* file, unsigned char* buffer) {
//     unsigned char byte = fgetc(file);
//     if(byte == EOF) return EOF;

//     // Get number of bytes in character
//     int byteCount = utf8ByteCount(byte);
//     if(byteCount == -1) {
//         fprintf(stderr, "Invalid UTF-8 start byte 0x%X\n", byte);
//         return -1;
//     }

//     // Initialise byte array
//     buffer[0] = byte;

//     // Get the next bytes
//     for(int i = 1; i < byteCount; i++) {
//         int nextByte = fgetc(file);

//         // Check if byte is invalid
//         if(nextByte == EOF || (nextByte & 0xC0) != 0x80) {
//             fprintf(stderr, "Invalid UTF-8 continuation byte 0x%X\n", byte);
//         }

//         buffer[i] = nextByte;
//     }

//     return byteCount;
// }

static void repl() {
    char line[1024];

    for(;;) {
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
        printf("Note: if using windows, terminal must be using code page 65001.\n");
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