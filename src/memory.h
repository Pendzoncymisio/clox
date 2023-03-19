#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"

/* Allocate memory for object
*   Arguments:
*   - type: of the object to allocate
*   - count: of the object to allocate
*
*   Return pointer to allocated memory
*/
#define ALLOCATE(type, count) \
(type*)reallocate(NULL, 0, sizeof(type) * (count))

/* Free memory of object
*   Arguments:
*   - type: of the object to free
*   - pointer: to the object
*
*   Return pointer, but no point in using it
*/
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

/* Calculate new capacity
*   Arguments:
*   - capacity: old capacity
*
*   Return what new capacity should be
*/
#define GROW_CAPACITY(capacity) \
((capacity) < 8 ? 8 : (capacity) * 2)

/* Grow array capacity
*   Arguments:
*   - type: of objects in array
*   - pointer: to the array to grow
*   - oldCount: count of objects in array
*   - newCount: new capacity of array
*
*   Return pointer to array
*/
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
(type*)reallocate(pointer, sizeof(type) * (oldCount), \
sizeof(type) * (newCount))

/* Free memory on an array
*   Arguments:
*   - type: of objects in array
*   - pointer: to the array to free
*   - oldCount: count of objects in array
*
*   Return pointer to array, but no point in using it
*/
#define FREE_ARRAY(type, pointer, oldCount) \
reallocate(pointer, sizeof(type) * (oldCount), 0)

/* Reallocates memory
*   Arguments:
*   - void* pointer: to the object to reallocate
*   - size_t oldSize
*   - size_t newSize
*
*   Return pointer to reallocated memory
*/
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

/* Mark object to not be sweeped by garbage collector
*   Arguments:
*   - Obj* object: to mark
*/
void markObject(Obj* object);

/* Mark value to not be sweeped by garbage collector
*   Arguments:
*   - Value value: to mark
*/
void markValue(Value value);

// Free objects that can't be rached by any part of the program
void collectGarbage();

// Free whole VM's objects linked list and grayStack
void freeObjects();

#endif