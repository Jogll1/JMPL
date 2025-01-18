#ifndef c_jmpl_common_h
#define c_jmpl_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC
#define UINT8_COUNT (UINT8_MAX + 1)

// ANSI Colours
#define RESET "\033[0m"
#define YELLOW "\033[33m"
#define RED "\033[31m"

#endif