#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"

/* Initialize array of values
*   Arguments:
*   - ValueArray* array: pointer to array to initialize
*/
void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

/* Write new value to values array
*   Arguments:
*   - ValueArray* array: pointer to write value to
*   - Value value: to write
*/
void writeValueArray(ValueArray* array, Value value) {

    // Grow array capacity if more space is required
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

/* Free memory of values array
*   Arguments:
*   - ValueArray* array: to be freed
*/
void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

/* Print value
*   Arguments:
*   - Value value: to be printed
*/
void printValue(Value value) {
    #ifndef NAN_BOXING
        switch (value.type) {
            case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
            case VAL_NIL: printf("nil"); break;
            case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
            case VAL_OBJ: printObject(value); break;
        }
    #else
        if (IS_BOOL(value)) {
            printf(AS_BOOL(value) ? "true" : "false");
        } else if (IS_NIL(value)) {
            printf("nil");
        } else if (IS_NUMBER(value)) {
            printf("%g", AS_NUMBER(value));
        } else if (IS_OBJ(value)) {
            printObject(value);
        }
    #endif
}

/* Check if values are equal
*   Arguments:
*   - Value a: first value to be compared
*   - Value b: second value to be compared
*   
*   Return whether values are equal
*/
bool valuesEqual(Value a, Value b) {
    #ifndef NAN_BOXING
        if (a.type != b.type) return false; // If different type then must be different

        // Casting to appropriate type and comparing then
        switch (a.type)
        {
            case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
            case VAL_NIL: return true;
            case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
            case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
            default: return false; //Unreachable
        }
    #else
        // If both values are numbers then casting to numbers and comparing that way to prevent NaN being equal to NaN
        if (IS_NUMBER(a) && IS_NUMBER(b)) {
            return AS_NUMBER(a) == AS_NUMBER(b);
        }
        return a == b; // All bytes needs to be equal for values other than numers to be equal
    #endif
}