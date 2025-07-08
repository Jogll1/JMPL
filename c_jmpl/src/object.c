#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include "../lib/c-stringbuilder/sb.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = true;

    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjClosure* newClosure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for(int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(NativeFn function, int arity) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->arity = arity;
    native->function = function;
    return native;
}

static ObjString* allocateString(unsigned char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NULL_VAL);
    pop();

    return string;
}

/**
 * @brief Hashes a char array using the FNV-1a hashing algorithm.
 * 
 * @param key    The char array that makes up the string
 * @param length The length of the string
 * @return       A hashed form of the string
 */
static uint32_t hashString(const unsigned char* key, int length) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }

    return hash;
}

ObjString* takeString(unsigned char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* copyString(const unsigned char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) return interned;

    unsigned char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);

    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash);
}

ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static void printFunction(ObjFunction* function) {
    if(function->name == NULL) {
        printf("<script>");
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

/**
 * @brief Returns the corresponding escaped character given a character.
 * 
 * @param esc The character to escape
 * @return    The escaped character
 */
static unsigned char decodeEscape(unsigned char esc) {
    switch (esc) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        default: return esc;
    }
}

/**
 * @brief Print an ObjString to the console.
 * 
 * @param string A pointer to an ObjString
 * 
 * Decodes each character in the string object, including escape characters.
 * For printing the raw characters, use printf().
 */
void printJMPLString(ObjString* string) {
    int length = string->length;
    unsigned char* chars = string->chars;

    char* result = malloc(length + 1);
    if (result == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    int ri = 0;

    for (int i = 0; i < length; i++) {
        // Add each string and decode escapes if necessary
        if (chars[i] == '\\' && i + 1 < length) {
            result[ri++] = decodeEscape(chars[i + 1]);
            i++; // Skip the escaped character
        } else {
            result[ri++] = chars[i];
        }
    }

    result[ri] = '\0';
    
    printf("%s", result);
    free(result);
}

void printObject(Value value) {
    switch(OBJ_TYPE(value)) {
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native>");
            break;
        case OBJ_STRING:
            printJMPLString(AS_STRING(value));
            break;
        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;
        case OBJ_SET:
            unsigned char* setString = setToString(AS_SET(value));
            printf("%s", setString);
            free(setString);
            break;
        case OBJ_TUPLE:
            unsigned char* tupleStr = tupleToString(AS_TUPLE(value));
            printf("%s", tupleStr);
            free(tupleStr);
            break;
        case OBJ_SET_ITERATOR:
            printf("<iterator>");
            break;
        default: 
            printf("<unknown>");
            return;
    }
}