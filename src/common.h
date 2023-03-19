#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING //Use NaN boxing for holding Value, for optimization purpose but might not work on all architectures
#define DEBUG_PRINT_CODE //Print opcodes when compiling
#define DEBUG_TRACE_EXECUTION //Print opcodes and stack when VM running

//#define DEBUG_STRESS_GC //Run garbage collector every time the memory is reallocated
//#define DEBUG_LOG_GC //Print garbage collector debug messages

#define UINT8_COUNT (UINT8_MAX + 1)

#endif