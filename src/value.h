#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifndef NAN_BOXING

typedef enum {
    VAL_BOOL,   // Boolean
    VAL_NIL,    // Nil
    VAL_NUMBER, // Number
    VAL_OBJ     // Object
} ValueType;

/* Value struct
*
*   Fields:
*   - ValueType type: of the value
*   - union as: content of the value, <type> field indicates how what type bytes should be read
*/
typedef struct {
    ValueType type; // of the value
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as; // content of the value, <type> field indicates how what type bytes should be read
} Value;

#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

#define AS_OBJ(value)       ((value).as.obj) // Treat value as object
#define AS_BOOL(value)      ((value).as.boolean) // Treat value as boolean
#define AS_NUMBER(value)    ((value).as.number) // Treat value as number

#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}}) // Type field is all we care about here
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#else // do NaN boxing

//NaN boxing is squeezing values other than number to bits remaining in NaN value
//SIGN_BIT and QNAN bits set -> Obj address
//QNAN bits set -> bool or nil values
//other bits combinations -> number value

#define SIGN_BIT    ((uint64_t)0x8000000000000000) // Mask for highest bit (sign bit)
#define QNAN        ((uint64_t)0x7ffc000000000000) // Mask for QNAN, leaving zeroed bits unused and available for NaN boxing

#define TAG_NIL 1   //01
#define TAG_FALSE 2 //10
#define TAG_TRUE 3  //11

typedef uint64_t Value;

#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL) // true and false value differ only by lowest bit
#define IS_NIL(value)       ((value) == NIL_VAL) // all bits must be the same
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN) // QNAN bits are not set, indicating number
#define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT)) // No bits inside QNAN and SIGN_BIT are 0, indicating Obj address

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    valueToNum(value)
#define AS_OBJ(value)       ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)         ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL           ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL            ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL             ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num)     numToValue(num)
#define OBJ_VAL(obj)        (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj)) // Object address with QNAN and SIGN bits set

/* Casts value to number
*   Arguments:
*   - Value value: to cast
*
*   Return casted number
*/
static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value)); // To satisfy all C type checkers
    return num;
}

/* Casts number to value
*   Arguments:
*   - double num: to cast
*
*   Return casted value
*/
static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double)); // To satisfy all C type checkers
    return value;
}

#endif

/* Struct for array of values
*
*   Fields:
*   - int capacity
*   - int count: of values within array
*   - Value* values: pointer to the first element
*/
typedef struct {
    int capacity;
    int count; // of values within array
    Value* values; // pointer to the first element
} ValueArray;

/* Check if values are equal
*   Arguments:
*   - Value a: first value to be compared
*   - Value b: second value to be compared
*   
*   Return whether values are equal
*/
bool valuesEqual(Value a, Value b);

/* Initialize array of values
*   Arguments:
*   - ValueArray* array: pointer to array to initialize
*/
void initValueArray(ValueArray* array);

/* Write new value to values array
*   Arguments:
*   - ValueArray* array: pointer to write value to
*   - Value value: to write
*/
void writeValueArray(ValueArray* array, Value value);

/* Free memory of values array
*   Arguments:
*   - ValueArray* array: to be freed
*/
void freeValueArray(ValueArray* array);

/* Print value
*   Arguments:
*   - Value value: to be printed
*/
void printValue(Value value);

#endif