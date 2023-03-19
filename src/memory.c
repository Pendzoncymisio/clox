#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2 // After running GC set next run when memory allocation crosses N * memory after this run

/* Reallocates memory
*   Arguments:
*   - void* pointer: to the object to reallocate
*   - size_t oldSize
*   - size_t newSize
*
*   Return pointer to reallocated memory
*/
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
        #ifdef DEBUG_STRESS_GC
            collectGarbage();
        #endif

        if (vm.bytesAllocated > vm.nextGC) {
            collectGarbage();
        }
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

/* Mark object to not be sweeped by garbage collector
*   Arguments:
*   - Obj* object: to mark
*/
void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

    #ifdef DEBUG_LOG_GC
        printf("%p mark ", (void*)object);
        printValue(OBJ_VAL(object));
        printf("\n");
    #endif

    object->isMarked = true;

    // Grow gray objects array if necessary
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        // Using C realloc not compiler's reallocate() not to trigger GC in GC run
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        if (vm.grayStack == NULL) exit(1);
    }

    // Add object to gray objects array
    vm.grayStack[vm.grayCount++] = object;
}

/* Mark value to not be sweeped by garbage collector
*   Arguments:
*   - Value value: to mark
*/
void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

/* Mark every value in array to not be sweeped by garbage collector
*   Arguments:
*   - ValueArray* array: to mark
*/
static void markArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

/* Blacken object from gray by marking and graying objects that can be referenced
*   Arguments:
*   - Obj* object: to be blackened
*/
static void blackenObject(Obj* object) {
    #ifdef DEBUG_LOG_GC
        printf("%p blacken ", (void*)object);
        printValue(OBJ_VAL(object));
        printf("\n");
    #endif
    // Marking objects if can be referenced
    switch (object->type)
    {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(bound->receiver); // Can reference instance
            markObject((Obj*)bound->method); // Can reference method itself
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            markObject((Obj*)klass->name); // Can reference its name
            markTable(&klass->methods); // Can reference all methods
            break;
        }
        case OBJ_CLOSURE:
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function); // Can reference its function
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]); // Can reference all upvalues
            }
            break;
        case OBJ_FUNCTION:
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);  // Can reference its name
            markArray(&function->chunk.constants); // Can reference constants it's owning
            break;
        case OBJ_INSTANCE:
            ObjInstance* instance = (ObjInstance*)object;
            markObject((Obj*)instance->klass); // Can reference parent class
            markTable(&instance->fields); // Can reference fields
            break;
        case OBJ_UPVALUE:
            markValue(((ObjUpvalue*)object)->closed); // Can reference closed values
            break;
        case OBJ_NATIVE: // Does not reference anything
        case OBJ_STRING: // Does not reference anything
            break;
    }
}

/* Free memory of object alonside accompanying heap allocated fields
*   Arguments:
*   - Obj* object: to be freed
*/
static void freeObject(Obj* object) {
    #ifdef DEBUG_LOG_GC
        printf("%p free type %d\n", (void*)object, object->type);
    #endif

    switch (object->type) {
        case OBJ_BOUND_METHOD:
            FREE(OBJ_BOUND_METHOD, object);
            break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_NATIVE: 
            FREE(OBJ_NATIVE, object);
            break;
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

// Mark objects that can be directly referenced by program
static void markRoots() {
    // Mark all values on current VM's stack
    for (Value* slot = vm.stack; slot<vm.stackTop; slot++) {
        markValue(*slot);
    }

    // Mark closures of all frames from callstack
    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    // Mark all upvalues from linked list of open upvalues
    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    markTable(&vm.globals); // Mark globals
    markCompilerRoots(); // Mark objects owned by compiler
    markObject((Obj*)vm.initString); // Mark "init" string
}

// Blacken objects from gray table until gray table empty
static void traceReferences() {
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

// Free objects that were not marked during mark step
static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    // Loop through whole linked list
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false; // Reset marking for the next GC run
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object; // Connect previous and next for linked list continuity
            } else {
                vm.objects = object; // If deleting first then set start of linked list as next object
            }

            freeObject(unreached);
        }
    }
}

// Free objects that can't be rached by any part of the program
void collectGarbage() {
    #ifdef DEBUG_LOG_GC
        printf("-- gc begin\n");
        size_t before = vm.bytesAllocated;
    #endif

    markRoots();
    traceReferences();
    /* Due to string interning strings cannot be marked during mark phase,
    as VM has a hash table with pointers to every string.
    Here we are removing from table any pointers that will be left dangling after sweep phase.
    */
    tableRemoveWhite(&vm.strings);
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

    #ifdef DEBUG_LOG_GC
        printf("-- gc end\n");
        printf(" collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
    #endif
}

// Free whole VM's objects linked list and grayStack
void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
    free(vm.grayStack);
}