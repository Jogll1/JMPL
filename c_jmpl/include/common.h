#ifndef c_jmpl_common_h
#define c_jmpl_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Debug

// #define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
// #define DEBUG_PRINT_TOKENS
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

// Args

#define NAN_BOXING

// Misc

#define UINT8_COUNT (UINT8_MAX + 1)
#define UNICODE_MAX (0x10FFFF)

// ANSI Colours

#define ANSI_RESET "\e[0m"
#define ANSI_YELLOW "\e[33m"
#define ANSI_RED "\e[31m"

// Error Codes

#define COMMAND_LINE_USAGE_ERROR 64
#define DATA_FORMAT_ERROR 65
#define INTERNAL_SOFTWARE_ERROR 70
#define IO_ERROR 74


#endif