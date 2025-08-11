#ifndef c_jmpl_string_h
#define c_jmpl_string_h

#include "object.h"

typedef struct GC GC;

typedef uint8_t  UCS1;
typedef uint16_t UCS2;
typedef uint32_t UCS4;

typedef enum {
    KIND_1_BYTE = 1, // Latin1 - Characters U+0000 - U+00FF
    KIND_2_BYTE = 2, // BMP    - Characters U+0000 - U+FFFF
    KIND_4_BYTE = 4  // All    - Characters U+0000 - U+10FFFF
} StringKind;

typedef struct {
    Obj obj;

    StringKind kind;
    size_t length;       // No. code points (characters)
    uint32_t hash;
    bool isAscii;

    union {
        UCS1* ucs1;
        UCS2* ucs2;
        UCS4* ucs4;
    } as;                // Code point array

    size_t utf8Length;    // No. bytes in UTF-8 (excluding null-terminator)
    unsigned char* utf8; // UTF-8 bytes cache (null-terminated) - NULL if not encoded yet or ucs1
} ObjUnicodeString;

// ========================================================================

struct ObjString {
    Obj obj;
    int length;           // No. bytes (need to change)
    unsigned char* chars; // UTF-8 bytes (null-terminated)
    uint32_t hash;
};

ObjString* takeString(GC* gc, unsigned char* chars, int length);
ObjString* copyString(GC* gc, const unsigned char* chars, int length);
ObjString* concatenateString(GC* gc, Value a, Value b);

void printJMPLString(ObjString* string);

#endif