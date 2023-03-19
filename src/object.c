#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

// Wrapper around allocateObject to make it type specific
#define ALLOCATE_OBJ(type, objectType) (type*)allocateObject(sizeof(type), objectType)

/* Allocate memory for an object
*   Arguments:
*   - size_t size: of object to allocate
*   - ObjType type
*
*   Return generic Obj pointer of allocated object. Returned object should be casted to appropriate object type
*/
static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm.objects;
    vm.objects = object;

    #ifdef DEBUG_LOG_GC
        printf("%p allocate %zu for %d\n", (void*)object, size, type);
    #endif

    return object;
}

/* Create bound method object
*   Arguments:
*   - Value receiver: instance to which method is bound. Should be of ObjInstance type
*   - ObjClosure* method: containing closed upvalues and function body of the method
*
*   Return newly created object
*/
ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD); // Allocate memory for object
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

/* Create class object
*   Arguments:
*   - ObjString* name
*
*   Return newly created object
*/
ObjClass* newClass(ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS); // Allocate memory for object
    klass->name = name;
    initTable(&klass->methods); // Initialize hash table for methods
    return klass;
}

/* Create closure object
*   Arguments:
*   - ObjFunction* function
*
*   Return newly created object
*/
ObjClosure* newClosure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount); // Allocate memory for upvalues
    for (int i = 0; i < function->upvalueCount; i++) { // Initialize upvalues with NULL value
        upvalues[i] = NULL;
    }
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE); // Alocate memory for object
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

/* Create function object
*
*   Return newly created object
*/
ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION); // Allocate memory for object
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk); // Init chunk for the function opcodes that will be added later
    return function;
}

/* Create instance object
*   Arguments:
*   - ObjClass* klass: class of the instance
*
*   Return newly created object
*/
ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE); // Allocate memory for object
    instance->klass = klass;
    initTable(&instance->fields); // Init hash table for instance fields
    return instance;
}

/* Create native function object
*   Arguments:
*   - NativeFn function: pointer to native function
*
*   Return newly created object
*/
ObjNative* newNative(NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE); // Allocate memory for object
    native->function = function;
    return native;
}

/* Allocate string on the heap
*   Arguments:
*   - char* chars: string itself
*   - int length: of string
*   - uint32_t hash
*
*   Return allocated object
*/
static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING); // Allocate memory for object
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string)); // Storing string on the stack so it won't be cleaned by GC if GC trigered during tableSet
    tableSet(&vm.strings, string, NIL_VAL); // Adding string to the VM's strings hash table
    pop(); // Popping from stack as storing no longer necessary
    return string;
}

/* Calculate string's hash using FNV-1a algorithm
*   Arguments:
*   - char* key: string to hash
*   - int length: of string to hash
*
*   Return calculated hash
*/
static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u; // Initial constant (FNV offset basis)
    for (int i = 0; i < length; i++) { // For every character
        hash ^= (uint8_t)key[i]; // XOR current hash value with character
        hash *= 16777619; // Prime to multiply with (FNV prime)
    }
    return hash;
}

/* Claim ownership of the passed string
*   Arguments:
*   - char* chars: string to be taken
*   - int length: of string
*
*   Return allocated object
*/
ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length); // Calculate hash

    // Try to find string in the vm's strings table. If found then free passed chars as string already interned
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    // Returning object without allocating new memory for <chars> effectively claiming ownership over them
    return allocateString(chars, length, hash);
}

/* Copy string to the heap without taking ownership
*   Arguments:
*   - char* chars: string to be copied
*   - int length: of string
*
*   Return allocated object
*/
ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length); // Calculate hash

    // Try to find string in the vm's strings table. If found then return interned string
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned; 

    // If string not interned allocate new memory and copy string
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0'; // Terminate string

    return allocateString(heapChars, length, hash);
}

/* Create upvalue object
*   Arguments:
*   - Value* slot: pointer to where the closed-over variable lives
*
*   Return newly created object
*/
ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE); // Allocate memory for object
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

/* Print function name
*   Arguments:
*   - ObjFunction* function
*/
static void printFunction(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

/* Print object
*   Arguments:
*   - Value value: to print, must be of VAL_OBJ type
*/
void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            printFunction(AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}