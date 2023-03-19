#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

VM vm; // Global VM

/* Native function - Calculate how long did it take since program start
*   Arguments:
*   - int argCount - arguments required by native wrapper, disregarded
*   - Value* args - arguments required by native wrapper, disregarded
*
*   Return number of seconds since program start
*/
static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// Reset global VM stack
static void resetStack() {
    vm.stackTop =  vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

/* Report runtime error, arguments behave like in printf
*   Arguments:
*   - const char* format: string to print
*   - ... - arbitrary number of arguments passed to vfprintf
*/
static void runtimeError(const char* format, ...) {
    // Handles "..."
    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args); // Print error message
    va_end(args);

    fputs("\n", stderr);

    // Print callstack
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    
    resetStack(); // Free memory of stack
}

/* Define native function in globals table
*   Arguments:
*   - const char* name: of the function in Lox
*   - NativeFn function: that should be wrapped
*/
static void defineNative(const char* name, NativeFn function) {
    // Push function and its name to stack, not to be cleaned by GC duing tableSet
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

// Initiaize global VM
void initVM() {
    resetStack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = copyString("init", 4); // Keep "init" string on heap for quick comparison when used in code

    defineNative("clock", clockNative);
}

// Free memory after VM
void freeVM() {
    freeTable(&vm.strings);
    freeTable(&vm.globals);
    vm.initString = NULL;
    freeObjects();
}

/* Push value onto VM's stack
*   Arguments:
*   - Value value: to push
*/
void push(Value value) {
    *vm.stackTop = value; // Add value at stack top
    vm.stackTop++;
}

/* Pop value from VM's stack
*
*   Return just popped value
*/
Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

/* Show what's on the stack without popping
*   Arguments:
*   - int distance: from top of the stack
*
*   Return value on the stack
*/
static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

/* Call function
*   Arguments:
*   - ObjClosure* closure: of called function
*   - int argCount: number of passed arguments
*
*   Return whether call successful
*/
static bool call(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d", closure->function->arity, argCount);
        return false;
    }
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm.frames[vm.frameCount++]; // Add new callframe
    frame->closure = closure;
    frame->ip = closure->function->chunk.code; // Initialize ip to point to the beginning of the function’s bytecode
    frame->slots = vm.stackTop - argCount - 1; // Point to slots of function being called and arguments
    return true;
}

/* Call value
*   Arguments:
*   - Value callee
*   - int argCount: number of passed arguments
*
*   Return whether call successful
*/
static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee))
        {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
            vm.stackTop[-argCount - 1] = bound->receiver; // Receiver instance should be placed by compiler on stack before callee
            return call(bound->method, argCount);
        }
        case OBJ_CLASS: {
            ObjClass* klass = AS_CLASS(callee);
            vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass)); // Place new instance before arguments
            Value initializer;
            if (tableGet(&klass->methods, vm.initString, &initializer)) { // Run initializer if exist
                return call(AS_CLOSURE(initializer), argCount);
            } else if (argCount != 0) { // If there is no initialzier arguments cannot be passed to class call
                runtimeError("Expected 0  arguments but got %d.", argCount);
                return false;
            }
            return true;
        }
        case OBJ_CLOSURE:
            return call(AS_CLOSURE(callee), argCount);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(argCount, vm.stackTop - argCount);
            vm.stackTop -= argCount + 1; // Clean function frame from the stack

            // Function compiled by clox have return operation for pushing result, native functions have to do it by themselves
            push(result);
            return true;
        }
        default:
            break; //Non-callable object type.
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

/* Invoke method from class
*   Arguments:
*   - ObjClass* klass: class of method
*   - ObjString* name: of method
*   - int argCount: number of passed arguments
*
*   Return whether invocation successful
*/
static bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

/* Invoke method from class
*   Arguments:
*   - ObjString* name: of method
*   - int argCount: number of passed arguments
*
*   Return whether invocation successful
*/
static bool invoke(ObjString* name, int argCount) {
    Value receiver = peek(argCount); // Look for receiver on stack before arguments

    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }

    return invokeFromClass(instance->klass, name, argCount);
}

/* Find method in class and push it to stack
*   Arguments:
*   - ObjClass* klass
*   - ObjString* name
*   
*   Return whether method found
*/
static bool bindMethod(ObjClass* klass, ObjString* name) {
    Value method;

    // Fields are checked before methods, so if we did not find the method then field with that name also does not exist
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method)); // Create method bound to instance from stack
    pop(); // Pop receiver instance
    push(OBJ_VAL(bound)); // Push new bound method
    return true;
}

/* Capture upvalue from one of enclosing functions
*   Arguments:
*   - Value* local: stack slot of variable to capture
*   
*   Return upvalue object
*/
static ObjUpvalue* captureUpvalue(Value* local) {
    // Initialize handler for linked list of openUpvalues
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;

    // As openUpvalues array is sorted by local address search for it untli higher addres or end of list found
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    // Check if upvalue with that address already exist, return it if so
    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    // Create new upvalue if previously does not exist
    ObjUpvalue* createdUpvalue = newUpvalue(local);

    // Insert upvalue in openUpvalues array
    createdUpvalue->next = upvalue;
    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

/* Moves upvalue from stack to the heap
*   Arguments:
*   - Value* last: slot index of lowest upvalue on stack to close
*/
static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location; // Copy the variable’s value into the closed field
        upvalue->location = &upvalue->closed; // Update that location to the address of the ObjUpvalue’s own closed field.
        vm.openUpvalues = upvalue->next; // Move to next open upvalue in the linked list
    }
}

/* Add method to the class's methods table
*   Arguments:
*   - ObjString* name: of the new method
*
*   Stack in:   class, method closure
*   Stack out:  class
*/
static void defineMethod(ObjString* name) {
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

/* Check if value is falsey
*   Arguments:
*   - Value value: to check
*
*   Return whether value falsey
*/
static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

/* Concatenate two top values from stack
*
*   Stack in:   value a, value b
*   Stack out:  result
*/
static void concatenate() {
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length; // Calculate how much memory needs to be allocated
    char* chars = ALLOCATE(char, length+1); 
    memcpy(chars, a->chars, a->length); // Copy first string
    memcpy(chars + a->length, b->chars, b->length); // Copy second string
    chars[length] = '\0'; // Terminate string

    ObjString* result = takeString(chars, length); // Take ownership of the string, as we've already allocated memory
    pop();
    pop();
    push(OBJ_VAL(result));
}

/* Main function for running VM's chunk
*
*   Return InterpretResult, INTERPRET_OK if everything ok
*/
static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1]; // Set current frame to the first element of frames array
    #define READ_BYTE() (*frame->ip++) // Read next byte from chunk
    #define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1])) // Read next two bytes as short number from chunk
    #define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()]) // Read next byte as address and dereference if from constants table
    #define READ_STRING() AS_STRING(READ_CONSTANT()) // Read next byte as string address in constants table
    /* Wrapper around simple binary operators for numbers
    Pops two topmost numbers from stack and pushes result of operator
    */
    #define BINARY_OP(valueType, op) \
        do { \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                runtimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push(valueType(a op b)); \
        } while (false)

    for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
        // Debug printing execution

        // Print stack before operation
        printf("          ");
        for(Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");

        // Print operation that will be executed
        disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
    #endif
        uint8_t instruction;

        // Opcodes behaviour also documented in chunk.h
        switch (instruction=READ_BYTE())
        {
        case OP_CONSTANT: { // Push constant from constants address to stack
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_NIL: push(NIL_VAL); break;
        case OP_TRUE: push(BOOL_VAL(true)); break;
        case OP_FALSE: push(BOOL_VAL(false)); break;
        case OP_POP: pop(); break;
        case OP_GET_LOCAL: { // Push local from chunk's slot index to stack
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            break;
        }
        case OP_SET_LOCAL: { // Update local on the slot from chunk with value from stack
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_GET_GLOBAL: { // Push global from globals table to stack
            ObjString* name = READ_STRING(); // Get string object from address specified by operand
            Value value;
            if (!tableGet(&vm.globals, name, &value)) { // Get value with name from globals hash table
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }
        case OP_DEFINE_GLOBAL: { // Define global in globals table
            ObjString* name = READ_STRING();
            tableSet(&vm.globals, name, peek(0)); // Peek rather than pop right away to not be freed by GC
            pop();
            break;
        }
        case OP_SET_GLOBAL: { // Set global value with value from stack
            ObjString* name = READ_STRING();
            if (tableSet(&vm.globals, name, peek(0))) {
                tableDelete(&vm.globals, name);
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_GET_UPVALUE: { // Get upvalue from upvalues table location
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case OP_SET_UPVALUE: { // Set upvalue value on the location with value from stack
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            break;
        }
        case OP_GET_PROPERTY: { // Get instance property
            if (!IS_INSTANCE(peek(0))) {
                runtimeError("Only instances have properties.");
                return INTERPRET_RUNTIME_ERROR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(0));
            ObjString* name = READ_STRING();
            Value value;
            if (tableGet(&instance->fields, name, &value)) {
                pop(); // Pop instance
                push(value); // Push property value
                break; // Finish resolving when found field
            }

            // Try finding method if field was not found
            if (!bindMethod(instance->klass, name)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SET_PROPERTY: { // Set instance from stack property with value with stack
            if (!IS_INSTANCE(peek(1))) {
                runtimeError("Only instances have fields.");
                return INTERPRET_RUNTIME_ERROR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(1));
            tableSet(&instance->fields, READ_STRING(), peek(0));
            Value value = pop(); // Pop value
            pop(); // Pop instance
            push(value); // Push value at the top of the stack as set statements should return what they evaluated to
            break;
        }
        case OP_GET_SUPER: { // Get method from superclass
            ObjString* name = READ_STRING();
            ObjClass* superclass = AS_CLASS(pop());
            if (!bindMethod(superclass, name)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_EQUAL: { // Check if values from stack equal
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
        case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
        case OP_ADD: { // Add two values from stack
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                concatenate();
            } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a+b));
            } else {
                runtimeError("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
        case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
        case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
        case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0))) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_PRINT: {
            printValue(pop());
            printf("\n");
            break;
        }
        case OP_JUMP: { // Unconditionally jump over number of instructions
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE: { // Jump over instructions if topmost value from stack false
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0))) frame->ip += offset;
            break;
        }
        case OP_LOOP: { // Jump backwards over instructions
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CALL: { // Call closure specified by adress from chunk
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1]; // callValue added frame to the frame-stack. Decreasing it back after call
            break;
        }
        case OP_INVOKE: { // Invoke method specified by string from chunk
            ObjString* method = READ_STRING();
            int argCount = READ_BYTE();
            if (!invoke(method, argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount-1]; // invoke added frame to the frame-stack. Decreasing it back after call
            break;
        }
        case OP_SUPER_INVOKE: { // Invoke method specified by string from chunk, from class at the top of the stack
            ObjString* method = READ_STRING();
            int argCount = READ_BYTE();
            ObjClass* superclass = AS_CLASS(pop());
            if (!invokeFromClass(superclass, method, argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1]; // invokeFromClass added frame to the frame-stack. Decreasing it back after call
            break;
        }
        case OP_CLOSURE: { // Create closure from function specified in chunk
            ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
            ObjClosure* closure = newClosure(function);
            push(OBJ_VAL(closure));

            // Loop through all upvalues
            for (int i = 0; i < closure->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) { // If local then capture
                    closure->upvalues[i] = captureUpvalue(frame->slots + index);
                } else { // If not local then should be already captured by enclosing function
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            break;
        }
        case OP_CLOSE_UPVALUE: // Move necessary upvalues from stack to heap
            closeUpvalues(vm.stackTop - 1);
            pop();
            break;
        case OP_RETURN: { // Clean local variables from stack and push function result. End run if outermost function
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        case OP_CLASS: // Push new class on stack
            push(OBJ_VAL(newClass(READ_STRING())));
            break;
        case OP_INHERIT: { // Add methods from superclass on stack to subclass on stack
            Value superclass = peek(1);
            if(!IS_CLASS(superclass)) {
                runtimeError("Superclass must be a class.");
                return INTERPRET_RUNTIME_ERROR;
            }

            ObjClass* subclass = AS_CLASS(peek(0));
            tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
            pop(); //Subclass.
            break;
        }
        case OP_METHOD: // Add method to class from stack
            defineMethod(READ_STRING());
            break;
        }
    }
// Clean up the macros
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

/* Compile and run source code
*   Arguments:
*   - const char* source: code to interpret
*
*   Return interpreting result
*/
InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}