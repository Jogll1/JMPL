#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#define CURRENT_VERSION "0.2.2"

static void repl() {
    char line[1024];

    while(true) {
        printf(ANSI_YELLOW ">> " ANSI_RESET);

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
        exit(IO_ERROR);
    }

    // Seek to the end and get how many bytes the file is
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    
    // Rewind file to the end
    rewind(file);

    // Allocate a string the size of the file
    unsigned char* buffer = (unsigned char*)malloc(fileSize + 1);

    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(IO_ERROR);
    }

    // Read the whole file
    size_t bytesRead = fread(buffer, sizeof(unsigned char), fileSize, file);

    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(IO_ERROR);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const unsigned char* path) {
    unsigned char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result != INTERPRET_OK) printf("Exited with code %d.\n", result);
    if (result == INTERPRET_COMPILE_ERROR) exit(DATA_FORMAT_ERROR);
    if (result == INTERPRET_RUNTIME_ERROR) exit(INTERNAL_SOFTWARE_ERROR);
}

int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {
        // If no file argument, run the REPL
        printf("JMPL v%s\n", CURRENT_VERSION);
        printf("Note: if using Windows, terminal must be using code page 65001 to properly display mathematical symbols.\n");
        repl();
    } else if(argc == 2) {
        // If there's a file argument, run the file
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: c_jmpl [path]\n");
        exit(COMMAND_LINE_USAGE_ERROR);
    }

    freeVM();
    return 0;
}