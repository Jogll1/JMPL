#ifndef c_jmpl_common_h
#define c_jmpl_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Debug

// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_PRINT_TOKENS
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

// Misc

#define UINT8_COUNT (UINT8_MAX + 1)
#define JMPL_PI 3.14159265358979323846
#define JMPL_EPSILON 1e-10

// ANSI Colours

#define ANSI_RESET "\033[0m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"

// Error Codes

#define COMMAND_LINE_USAGE_ERROR 64
#define DATA_FORMAT_ERROR 65
#define INTERNAL_SOFTWARE_ERROR 70
#define IO_ERROR 74


#endif