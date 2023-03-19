#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

/* Debug print all instructions in a chunk
*   Arguments:
*   - Chunk* chunk: to be disassembled
*   - const char* name: of the chunk
*/
void disassembleChunk(Chunk* chunk, const char* name);

/* Debug print one instruction
*   Arguments:
*   - Chunk* chunk: from which instruction originate
*   - int offset: of the instruction
*
*   Return offset incremented by correct amount dependent on number of operands
*/
int disassembleInstruction(Chunk* chunk, int offset);

#endif