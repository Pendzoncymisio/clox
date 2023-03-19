#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

// Handles REPL session by reading and interpreting code line by line
static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

/* Read file from given path
*   Arguments:
*   - const char* path: to file
*
*   Return pointer to allocated string with source code
*/
static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");

    if (file == NULL) { // Could not open file on a given path
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    
    // Seek to the end of the file to check how big the file is for the allocation
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1); // Allocate required memory
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file); // Read file content to the buffer
    if(bytesRead < fileSize) { // Could not read whole file
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0'; // Terminate string

    fclose(file);
    return buffer;
}

/* Interpret and run file from given path
*   Arguments:
*   - const char* path: to file
*/
static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

// Program entry
int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {  // Start repl session if no script path specified
        repl();
    } else if (argc == 2) { // Compile and run specified script
        runFile(argv[1]);
    } else { // Too much arguments passed
        fprintf(stderr, "Usage: clox [path]\n"); 
        exit(64);
    }

    freeVM();
    return 0;
}