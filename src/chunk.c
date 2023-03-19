#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

/* Init chunk that will hold the programs
*   Arguments:
*   - Chunk* chunk: declared pointer to Chunk to be initialized
*/
void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants); //Initialize constants array
}

/* Free chunk's memory
*   Arguments:
*   - Chunk* chunk: pointer to Chunk to be freed
*/
void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity); //Free opcodes array
    FREE_ARRAY(int, chunk->lines, chunk->capacity); //Free array of lines numbers
    freeValueArray(&chunk->constants); //Free constants array
    initChunk(chunk);
}

/* Add byte to chunk
*   Arguments:
*   - Chunk* chunk: pointer to Chunk to write
*   - uint8_t byte: opcode or operator to be appended
*   - int line: source code line from which operation originates
*/
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) { //Grow capacity if too low to write next byte
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

/* Add constant to chunk
*   Arguments:
*   - Chunk* chunk: pointer to Chunk to write
*   - Value value: which will be added to constants table of chunk
*/
int addConstant(Chunk* chunk, Value value) {
    push(value); //Storing value on stack temporarly to preserve it from garbage collector during array writing
    writeValueArray(&chunk->constants, value); //Add value to chunk's constant table
    pop();
    return chunk->constants.count - 1;
}