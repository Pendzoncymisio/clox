#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type) // Get type of object

// Checks if object is of any given type
#define IS_BOUND_METHOD(value)  isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)         isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)       isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)      isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)      isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)        isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)

// Casts of objects to given type
#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)         ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)      ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)        (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_BOUND_METHOD,   // Bound method
    OBJ_CLASS,          // Class
    OBJ_CLOSURE,        // Closure
    OBJ_FUNCTION,       // Function
    OBJ_INSTANCE,       // Instance
    OBJ_NATIVE,         // Native function
    OBJ_STRING,         // String
    OBJ_UPVALUE         // Upvalue
} ObjType;

/* Objects header struct
*
*   Fields:
*   - ObjType type
*   - bool isMarked: as accessible to not be freed by GC
*   - struct Obj* next: object in the linked list
*/
struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};

/* Function object struct
*
*   Fields:
*   - Obj obj: object header
*   - int arity: of the function (number of arguments)
*   - int upvalueCount: number of function upvalues
*   - Chunk chunk: of the function opcodes to execute
*   - ObjString* name: of the function
*/
typedef struct {
    Obj obj; // object header
    int arity; // of the function (number of arguments)
    int upvalueCount; // number of function upvalues
    Chunk chunk; // of the function opcodes to execute
    ObjString* name; // of the function
} ObjFunction;

// Pointer to native function
typedef Value (*NativeFn)(int argCount, Value* args);

/* Native function object struct
*
*   Fields:
*   - Obj obj: object header
*   - NativeFn function
*/
typedef struct {
    Obj obj; // object header
    NativeFn function;
} ObjNative;

/* String object struct
*
*   Fields:
*    Obj obj: object header
*    int length: of the string
*    char* chars: pointer to first character
*    uint32_t hash
*/
struct ObjString {
    Obj obj; // object header
    int length; // of the string
    char* chars; // pointer to first character
    uint32_t hash;
};

/* Upvalue object struct
*
*   Fields:
*    Obj obj: object header
*    Value* location: pointer to closed-over variable
*    Value closed: storage of closed value on the heap
*    struct ObjUpvalue* next: upvalue in the linked list
*/
typedef struct ObjUpvalue {
    Obj obj; // object header
    Value* location; // pointer to closed-over variable
    Value closed; // storage of closed value on the heap
    struct ObjUpvalue* next; // upvalue in the linked list
} ObjUpvalue;

/* Closure function object struct
*
*   Fields:
*   Obj obj: object header
*   ObjFunction* function
*   ObjUpvalue** upvalues: pointer to the first element of upvalues linked list
*   int upvalueCount: number of upvalues in linked list
*/
typedef struct {
    Obj obj; // object header
    ObjFunction* function;
    ObjUpvalue** upvalues; // pointer to the first element of upvalues linked list
    int upvalueCount; // number of upvalues in linked list captured by closure
} ObjClosure;

/* Class object struct
*
*   Fields:
*   Obj obj: object header
*   ObjString* name
*   Table methods: hash table of class methods
*/
typedef struct {
    Obj obj; // object header
    ObjString* name;
    Table methods; // hash table of class methods
} ObjClass;

/* Class object struct
*
*   Fields:
*   Obj obj: object header
*   ObjClass* klass: parent class
*   Table fields: of the instance
*/
typedef struct {
    Obj obj; // object header
    ObjClass* klass; // parent class
    Table fields; // of the instance
} ObjInstance;

/* Class object struct
*
*   Fields:
*   Obj obj: object header
*   Value receiver: instance to which method is bound. Should be of ObjInstance type
*   ObjClosure* method: containing closed upvalues and function body of the method
*/
typedef struct {
    Obj obj; // object header
    Value receiver; // instance to which method is bound. Should be of ObjInstance type
    ObjClosure* method; // containing closed upvalues and function body of the method
} ObjBoundMethod;

/* Create bound method object
*   Arguments:
*   - Value receiver: instance to which method is bound. Should be of ObjInstance type
*   - ObjClosure* method: containing closed upvalues and function body of the method
*
*   Return newly created object
*/
ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);

/* Create class object
*   Arguments:
*   - ObjString* name
*
*   Return newly created object
*/
ObjClass* newClass(ObjString* name);

/* Create closure object
*   Arguments:
*   - ObjFunction* function
*
*   Return newly created object
*/
ObjClosure* newClosure(ObjFunction* function);

/* Create function object
*
*   Return newly created object
*/
ObjFunction* newFunction();

/* Create instance object
*   Arguments:
*   - ObjClass* klass: class of the instance
*
*   Return newly created object
*/
ObjInstance* newInstance(ObjClass* klass);

/* Create native function object
*   Arguments:
*   - NativeFn function: pointer to native function
*
*   Return newly created object
*/
ObjNative* newNative(NativeFn function);

/* Claim ownership of the passed string
*   Arguments:
*   - char* chars: string to be taken
*   - int length: of string
*
*   Return allocated object
*/
ObjString* takeString(char* chars, int length);

/* Copy string to the heap without taking ownership
*   Arguments:
*   - char* chars: string to be copied
*   - int length: of string
*
*   Return allocated object
*/
ObjString* copyString(const char* chars, int length);

/* Create upvalue object
*   Arguments:
*   - Value* slot: pointer to where the closed-over variable lives
*
*   Return newly created object
*/
ObjUpvalue* newUpvalue(Value* slot);

/* Print object
*   Arguments:
*   - Value value: to print, must be of VAL_OBJ type
*/
void printObject(Value value);

/* Check if the object is of given type
*   Arguments:
*   - Value value: to check
*   - ObjType type: to compare
*
*   Return true if object of a given type
*/
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif