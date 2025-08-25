#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "utils.h"
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

static void runFile(const unsigned char* path) {
    unsigned char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result != INTERPRET_OK) printf("Exited with code %d.\n", result);
    if (result == INTERPRET_COMPILE_ERROR) exit(DATA_FORMAT_ERROR);
    if (result == INTERPRET_RUNTIME_ERROR) exit(INTERNAL_SOFTWARE_ERROR);
}

int main(int argc, const char* argv[]) {
    srand((unsigned int)time(NULL));

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