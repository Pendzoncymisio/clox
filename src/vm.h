#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64 // Maksimum depth of callstack
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT) // Maximum depth of stack

/* Callframe struct for single ongoing function call
*
*   Fields:
*   - ObjClosure* closure: pointer to function being called
*   - uint8_t* ip: instruction pointer
*   - Value* slots: pointer into VM's value stack that function can use
*/
typedef struct {
    ObjClosure* closure; // pointer to function being called
    uint8_t* ip; // instruction pointer
    Value* slots; // pointer into VM's value stack that function can use
} CallFrame;

/* Main VM that process opcodes during runtime
*
*   Fields:
*   - CallFrame frames[FRAMES_MAX]: callstack of currently executed function
*   - int frameCount: depth of current callstack
*   - Value stack[STACK_MAX]: main stack of the VM
*   - Value* stackTop: pointer to current top of the stack
*   - Table globals: hash table of global variables
*   - Table strings: hash table of interned strings
*   - ObjString* initString: just "init", so comparison will be fast
*   - ObjUpvalue* openUpvalues: pointer to the start of linked list of open upvalues
*   - size_t bytesAllocated: all bytes allocated for memory, for GC purposes
*   - size_t nextGC: threshold of allocated memory to trigger garbage collection
*   - Obj* objects: pointer to the start of linked list of objects owned by VM
*   - int grayCount: number of currently grayed objects, for GC purposes
*   - int grayCapacity: capacity of grayStack, for GC purposes
*   - Obj** grayStack: stack of pointers to grayed objects, for GC purposes
*/
typedef struct {
    CallFrame frames[FRAMES_MAX]; // callstack of currently executed function
    int frameCount; // depth of current callstack

    Value stack[STACK_MAX]; // main stack of the VM
    Value* stackTop; // pointer to current top of the stack
    Table globals; // hash table of global variables
    Table strings; // hash table of interned strings
    ObjString* initString; // just "init", so comparison will be fast
    ObjUpvalue* openUpvalues; // pointer to the start of linked list of open upvalues

    size_t bytesAllocated; //  all bytes allocated for memory, for GC purposes
    size_t nextGC; // threshold of allocated memory to trigger garbage collection
    Obj* objects; // pointer to the start of linked list of objects owned by VM

    int grayCount; // number of currently grayed objects, for GC purposes
    int grayCapacity; // capacity of grayStack, for GC purposes
    Obj** grayStack; //stack of pointers to grayed objects, for GC purposes
} VM;

typedef enum {
    INTERPRET_OK,           // Interpretation successful
    INTERPRET_COMPILE_ERROR,// Compilation error
    INTERPRET_RUNTIME_ERROR // Runtime error
} InterpretResult;

extern VM vm; // Global VM

// Init VM that will hold opcodes and runtime objects
void initVM();

// Free VM's memory
void freeVM();

/* Compile and run source code
*   Arguments:
*   - const char* source: code to interpret
*
*   Return interpreting result
*/
InterpretResult interpret(const char* source);

/* Push value onto VM's stack
*   Arguments:
*   - Value value: to push
*/
void push(Value value);

/* Pop value from VM's stack
*
*   Return just popped value
*/
Value pop();

#endif