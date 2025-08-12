#ifndef c_jmpl_string_h
#define c_jmpl_string_h

#include "object.h"

typedef struct GC GC;

typedef uint8_t  UCS1;
typedef uint16_t UCS2;
typedef uint32_t UCS4;

/**
 * The kind of a string is determined by the size of the largest code point.
 * KIND_ASCII  = 0, All characters U+0000 - U+007F
 * KIND_1_BYTE = 1, All characters U+0000 - U+00FF, at least one >= U+0080
 * KIND_2_BYTE = 2, All characters U+0000 - U+FFFF, at least one >= U+0100
 * KIND_4_BYTE = 4  All characters U+0000 - U+10FFFF, at least one >= U+10000
 */
typedef enum {
    KIND_ASCII  = 0, // ASCII  - Characters U+0000 - U+007F
    KIND_1_BYTE = 1, // Latin1 - Characters U+0000 - U+00FF
    KIND_2_BYTE = 2, // BMP    - Characters U+0000 - U+FFFF
    KIND_4_BYTE = 4  // All    - Characters U+0000 - U+10FFFF
} StringKind;

struct ObjUnicodeString {
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
} ;

void freeUnicodeString(GC* gc, ObjUnicodeString* string);

ObjUnicodeString* copyUnicodeString(GC* gc, const unsigned char* utf8, int utf8Length);
ObjUnicodeString* concatenateUnicodeString(GC* gc, Value a, Value b);

Value indexString(ObjUnicodeString* string, size_t index);
void printJMPLString(ObjUnicodeString* string);

#endif