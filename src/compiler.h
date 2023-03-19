#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

/* Compile source code to opcodes
*   Arguments:
*   - const char* source: pointer to the source code
*
*   Return script function that can be run by VM
*/
ObjFunction* compile(const char* source);

// Mark functions on compile stack to not be sweeped by garbage collector
void markCompilerRoots();

#endif