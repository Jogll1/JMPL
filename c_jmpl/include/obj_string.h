#ifndef c_jmpl_string_h
#define c_jmpl_string_h

#include "object.h"
#include "hash.h"

typedef struct GC GC;

typedef uint8_t  UCS1;
typedef uint16_t UCS2;
typedef uint32_t UCS4;

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
    hash_t hash;

    union {
        UCS1* ucs1;
        UCS2* ucs2;
        UCS4* ucs4;
    } as; // Code point array

    size_t utf8Length;   // No. bytes in UTF-8 (excluding null-terminator)
    unsigned char* utf8; // UTF-8 bytes cache (null-terminated) - NULL if not encoded yet or ASCII
};

// --- Strings ---

void freeString(GC* gc, ObjString* string);

ObjString* copyString(GC* gc, const unsigned char* utf8, int utf8Length);
ObjString* concatenateStringsHelper(GC* gc, Value a, Value b);

Value indexString(ObjString* string, int index);
ObjString* sliceString(GC* gc, ObjString* string, int start, int end);

void printJMPLString(ObjString* string);

#endif