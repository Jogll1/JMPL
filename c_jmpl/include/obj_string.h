#ifndef c_jmpl_string_h
#define c_jmpl_string_h

#include "object.h"

typedef struct GC GC;

struct ObjString {
    Obj obj;
    int length;
    unsigned char* chars;
    uint32_t hash;
};

ObjString* takeString(GC* gc, unsigned char* chars, int length);
ObjString* copyString(GC* gc, const unsigned char* chars, int length);

void printJMPLString(ObjString* string);

#endif