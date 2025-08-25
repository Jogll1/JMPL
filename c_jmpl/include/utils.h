#ifndef c_jmpl_utils_h
#define c_jmpl_utils_h

#include <stdint.h>

#include "common.h"

// I/O

bool getAbsolutePath(const char* path, char* resolved);
void getFileName(const char* path, char* name, size_t resolvedSize);
unsigned char* readFile(const char* path);

// Escape Sequences

/**
 * The type of an escape sequence.
 * ESC_SIMPLE     = \a, \b, \e, \f, \n, \r, \t, \v, \\, \', \", \0
 * ESC_HEX        = \xhh (where hh is a byte in hex)
 * ESC_UNICODE    = \uhhhh (where hhhh is a code point < U+100000)
 * ESC_UNICODE_LG = \Uhhhhhh (where hhhhhh is a code point > U+100000)
 * ESC_INVALID    = Invalid
 */
typedef enum {
    ESC_SIMPLE = 1,
    ESC_HEX = 2,
    ESC_UNICODE = 4,
    ESC_UNICODE_LG = 6,
    ESC_INVALID
} EscapeType;

unsigned char decodeSimpleEscape(unsigned char esc);
EscapeType getEscapeType(unsigned char esc);

// Hex

bool isHex(unsigned char c);
int hexToValue(unsigned char c);

// Unicode

#define ASCII_MAX (0x007F)
#define UTF8_2_BYTE_MAX (0x07FF)
#define UTF8_3_BYTE_MAX (0xFFFF)

#define UNICODE_MAX (0x10FFFF)

uint32_t utf8ToUnicode(const unsigned char* input, int numBytes);
int8_t unicodeToUtf8(uint32_t codePoint, unsigned char* output);
int8_t getCharByteCount(unsigned char byte);
int8_t getCodePointByteCount(uint32_t codePoint);

#endif