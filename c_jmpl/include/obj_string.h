#ifndef c_jmpl_string_h
#define c_jmpl_string_h

#include "object.h"

typedef struct GC GC;

enum StringKind {
    KIND_ASCII,  // ASCII  - Characters U+0000 - U+007F
    KIND_1_BYTE, // Latin1 - Characters U+0000 - U+00FF
    KIND_2_BYTE, // BMP    - Characters U+0000 - U+FFFF
    KIND_4_BYTE  // All    - Characters U+0000 - U+10FFFF
};

struct ObjString {
    Obj obj;
    int length;           // No. bytes (so far)
    unsigned char* chars; // UTF-8 bytes (null-terminated)
    uint32_t hash;
};

ObjString* takeString(GC* gc, unsigned char* chars, int length);
ObjString* copyString(GC* gc, const unsigned char* chars, int length);
ObjString* concatenateString(GC* gc, Value a, Value b);

void printJMPLString(ObjString* string);

#endif