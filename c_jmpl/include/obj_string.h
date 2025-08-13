#ifndef c_jmpl_string_h
#define c_jmpl_string_h

#include "object.h"

typedef struct GC GC;

typedef uint8_t  UCS1;
typedef uint16_t UCS2;
typedef uint32_t UCS4;

/**
 * The type of an escape sequence.`
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

/**
 * The kind of a string is determined by the size of the largest code point.
 * KIND_ASCII  = 0, ASCII  - All characters U+0000 - U+007F
 * KIND_1_BYTE = 1, Latin1 - All characters U+0000 - U+00FF, at least one >= U+0080
 * KIND_2_BYTE = 2, BMP    - All characters U+0000 - U+FFFF, at least one >= U+0100
 * KIND_4_BYTE = 4  All    - All characters U+0000 - U+10FFFF, at least one >= U+10000
 */
typedef enum {
    KIND_ASCII  = 0,
    KIND_1_BYTE = 1,
    KIND_2_BYTE = 2,
    KIND_4_BYTE = 4 
} StringKind;

struct ObjString {
    Obj obj;

    StringKind kind;
    size_t length; // No. code points (characters)
    uint32_t hash;

    union {
        UCS1* ucs1;
        UCS2* ucs2;
        UCS4* ucs4;
    } as; // Code point array

    size_t utf8Length;   // No. bytes in UTF-8 (excluding null-terminator)
    unsigned char* utf8; // UTF-8 bytes cache (null-terminated) - NULL if not encoded yet or ASCII
};

// --- Escape Sequences ---
bool isHex(unsigned char c);
int hexToValue(unsigned char c);
unsigned char decodeSimpleEscape(unsigned char esc);
EscapeType getEscapeType(unsigned char esc);

// --- Strings --- 
void freeString(GC* gc, ObjString* string);

ObjString* copyString(GC* gc, const unsigned char* utf8, int utf8Length);
ObjString* concatenateString(GC* gc, Value a, Value b);

Value indexString(ObjString* string, size_t index);
void printJMPLString(ObjString* string);

#endif