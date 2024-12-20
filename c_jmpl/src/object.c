#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;

    return object;
}

static ObjString* allocateString(unsigned char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;

    return string;
}

ObjString* takeString(unsigned char* chars, int length) {
    return allocateString(chars, length);
}

ObjString* copyString(const unsigned char* chars, int length) {
    unsigned char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(heapChars, length);
}

/**
 * Converts a value to an ObjString.
 * 
 * @param value the value to convert
 * @return      a pointer to an ObjString
 */
ObjString* valueToString(Value value) {
    if(IS_STRING(value)) {
        return AS_STRING(value);
    }

    unsigned char* str;
    switch(value.type) {
        case VAL_BOOL:
            str = BOOL_TO_STRING(AS_BOOL(value));
            break;
        case VAL_NULL:
            str = NULL_TO_STRING;
            break;
        case VAL_NUMBER:
            NUMBER_TO_STRING(AS_NUMBER(value), &str);
            break;
        default: return NULL;
    }

    ObjString* result = AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    // free(str);
    return result;
}

void printObject(Value value) {
    switch(OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        default: return;
    }
}