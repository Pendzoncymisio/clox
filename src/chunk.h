#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

//Enum of available opcodes
typedef enum {
    /*Chunk:    OP_CONSTANT, value
    Stack in:
    Stack out:  value
    */
    OP_CONSTANT,
    /*Chunk:    OP_NIL
    Stack in:
    Stack out:  NIL_VAL
    */       
    OP_NIL,
    /*Chunk:    OP_TRUE
    Stack in:
    Stack out:  true
    */
    OP_TRUE,
    /*Chunk:    OP_FALSE
    Stack in:
    Stack out:  false
    */
    OP_FALSE,
    /*Chunk:    OP_POP
    Stack in:   anything
    Stack out:  
    */
    OP_POP,
    /*Chunk:    OP_GET_LOCAL, frame slot addr
    Stack in:
    Stack out:  value
    */
    OP_GET_LOCAL,
    /*Chunk:    OP_SET_LOCAL, frame slot addr
    Stack in:   value
    Stack out:  value
    Value also set at the frame slot addr
    */
    OP_SET_LOCAL,
    /*Chunk:    OP_GET_GLOBAL, global name pointer
    Stack in:
    Stack out:  value
    */
    OP_GET_GLOBAL,
    /*Chunk:    OP_DEFINE_GLOBAL, global name pointer
    Stack in:   value
    Stack out:
    Value set in globals table
    */
    OP_DEFINE_GLOBAL,
    /*Chunk:    OP_SET_GLOBAL, global name pointer
    Stack in:   value
    Stack out:  value
    Value set in globals table only if previously defined
    */
    OP_SET_GLOBAL,
    /*Chunk:    OP_GET_UPVALUE, closure upvalues slot addr
    Stack in:   
    Stack out:  upvalue location
    */
    OP_GET_UPVALUE,
    /*Chunk:    OP_SET_UPVALUE, closure upvalues slot addr
    Stack in:   upvalue location
    Stack out:  upvalue location
    Upvalue location set in current closure upvalues table
    */
    OP_SET_UPVALUE,
    /*Chunk:    OP_GET_PROPERTY, property name pointer
    Stack in:   instance pointer
    Stack out:  field/bound method pointer
    */
    OP_GET_PROPERTY,
    /*Chunk:    OP_SET_PROPERTY, property name pointer
    Stack in:   instance pointer, value
    Stack out:  value
    Property set in instance fields table
    */
    OP_SET_PROPERTY,
    /*Chunk:    OP_GET_SUPER, property name pointer
    Stack in:   superclass pointer
    Stack out:  bound method pointer
    */
    OP_GET_SUPER,
    /*Chunk:    OP_EQUAL
    Stack in:   value, value
    Stack out:  bool value
    */
    OP_EQUAL,
    /*Chunk:    OP_GREATER
    Stack in:   number value, number value
    Stack out:  bool value
    */
    OP_GREATER,
    /*Chunk:    OP_LESS
    Stack in:   number value, number value
    Stack out:  bool value
    */
    OP_LESS,
    /*Chunk:    OP_ADD
    Stack in:   number value or string pointer, number value or string pointer
    Stack out:  number value or string pointer
    */
    OP_ADD,
    /*Chunk:    OP_SUBTRACT
    Stack in:   number value, number value
    Stack out:  number value
    */
    OP_SUBTRACT,
    /*Chunk:    OP_MULTIPLY
    Stack in:   number value, number value
    Stack out:  number value
    */
    OP_MULTIPLY,
    /*Chunk:    OP_DIVIDE
    Stack in:   number value, number value
    Stack out:  number value
    */
    OP_DIVIDE,
    /*Chunk:    OP_NOT
    Stack in:   value
    Stack out:  bool value
    */
    OP_NOT,
    /*Chunk:    OP_NEGATE
    Stack in:   number value
    Stack out:  number value
    */
    OP_NEGATE,
    /*Chunk:    OP_PRINT
    Stack in:   value
    Stack out:
    Print value
    */
    OP_PRINT,
    /*Chunk:    OP_JUMP, offset
    Stack in:   
    Stack out:
    Increment instruction pointer by offset
    */
    OP_JUMP,
    /*Chunk:    OP_JUMP_IF_FALSE, offset
    Stack in:   value
    Stack out:  value
    Increment instruction pointer by offset if value false
    */
    OP_JUMP_IF_FALSE,
    /*Chunk:    OP_LOOP, offset
    Stack in:   
    Stack out:
    Decrement instruction pointer by offset
    */
    OP_LOOP,
    /*Chunk:    OP_CALL, arguments count
    Stack in:   [instance], callee pointer, arg1 ... argN
    Stack out:  [new instance], result
    If function then move instruction pointer to the callee chunk and start executing
    */
    OP_CALL,
    /*Chunk:    OP_INVOKE, method name pointer, arguments count
    Stack in:   instance, arg1 ... argN
    Stack out:  result
    */
    OP_INVOKE,
    /*Chunk:    OP_SUPER_INVOKE, method name pointer, arguments count
    Stack in:   instance, arg1 ... argN, superclass
    Stack out:  result
    */
    OP_SUPER_INVOKE,
    /*Chunk:    OP_CLOSURE, closure pointer, isLocal1, index1 ... isLocalN, indexN
    Stack in:   
    Stack out:
    Arguments count stored in closure object
    */
    OP_CLOSURE,
    /*Chunk:    OP_CLOSE_UPVALUE
    Stack in:   upvalue
    Stack out:  
    Upvalue moved to the heap
    */
    OP_CLOSE_UPVALUE,
    /*Chunk:    OP_RETURN
    Stack in:   result
    Stack out:  result
    Get out of callframe, clean locals from stack
    */
    OP_RETURN,
    /*Chunk:    OP_CLASS
    Stack in:   class name
    Stack out:  
    */
    OP_CLASS,
    /*Chunk:    OP_INHERIT
    Stack in:   superclass, receiver class
    Stack out:  
    */
    OP_INHERIT,
    /*Chunk:    OP_METHOD
    Stack in:   class, method closure
    Stack out:  class
    */
    OP_METHOD
} OpCode;

/* Chunk struct
*
*   Fields:
*   - int count: of bytes within chunk
*   - int capacity
*   - uint8_t* code: pointer to array of opcodes
*   - int* lines: pointer to array of lines corresponding to opcodes
*   - ValueArray constants
*/
typedef struct {
    int count; // of bytes within chunk
    int capacity;
    uint8_t* code; // pointer to array of opcodes
    int* lines; // pointer to array of lines corresponding to opcodes
    ValueArray constants;
} Chunk;

/* Init chunk that will hold the programs
*   Arguments:
*   - Chunk* chunk: declared pointer to Chunk
*/
void initChunk(Chunk* chunk);

/* Free chunk's memory
*   Arguments:
*   - Chunk* chunk: pointer to Chunk to be freed
*/
void freeChunk(Chunk* chunk);

/* Add byte to chunk
*   Arguments:
*   - Chunk* chunk: pointer to Chunk to write
*   - uint8_t byte: opcode or operator to be appended
*   - int line: source code line from which operation originates
*/
void writeChunk(Chunk* chunk, uint8_t byte, int line);

/* Add constant to chunk
*   Arguments:
*   - Chunk* chunk: pointer to Chunk to write
*   - Value value: which will be added to constants table of chunk
*/
int addConstant(Chunk* chunk, Value value);

#endif

