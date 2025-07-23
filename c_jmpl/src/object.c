#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "memory.h"
#include "object.h"
#include "set.h"
#include "tuple.h"
#include "table.h"
#include "value.h"
#include "gc.h"
#include "../lib/c-stringbuilder/sb.h"

#define ALLOCATE_OBJ(gc, type, objectType) \
    (type*)allocateObject(gc, sizeof(type), objectType)

Obj* allocateObject(GC* gc, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(gc, NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = gc->objects;
    gc->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjClosure* newClosure(GC* gc, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(gc, ObjUpvalue*, function->upvalueCount);
    for(int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(gc, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction(GC* gc) {
    ObjFunction* function = ALLOCATE_OBJ(gc, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(GC* gc, NativeFn function, int arity) {
    ObjNative* native = ALLOCATE_OBJ(gc, ObjNative, OBJ_NATIVE);
    native->arity = arity;
    native->function = function;
    return native;
}

static ObjString* allocateString(GC* gc, int length, uint32_t hash) {
    ObjString* string = (ObjString*)allocateObject(gc, sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    string->chars[0] = '\0';
    string->hash = hash;

    // push(OBJ_VAL(string));
    // tableSet(&vm.strings, string, NULL_VAL);
    // pop();

    return string;
}

#define INIT_HASH 2166136261u

/**
 * @brief Hashes a char array using the FNV-1a hashing algorithm.
 * 
 * @param hash   The starting hash
 * @param key    The char array that makes up the string
 * @param length The length of the string
 * @return       A hashed form of the string
 */
static uint32_t hashString(uint32_t hash, const unsigned char* key, int length) {
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }

    return hash;
}

ObjString* concatStrings(GC* gc, Table* strings, const char* a, int aLen, uint32_t aHash, const char* b, int bLen) {
    assert(aLen + bLen >= 0);

    uint32_t hash = hashString(aHash, b, bLen);
    Entry* entry = tableJoinedStringsEntry(gc, strings, a, aLen, b, bLen, hash);
    if (entry->key != NULL) {
        // Concatenated string already interned
        return entry->key;
    }

    int length = aLen + bLen;
    ObjString* string = allocateString(gc, length, hash);
    memcpy(string->chars, a, aLen);
    memcpy(string->chars + aLen,b, bLen);
    string->chars[length] = '\0';

    tableSetEntry(strings, entry, string, NULL_VAL);
    return string;
}

ObjString* copyString(GC* gc, Table* strings, const unsigned char* chars, int length) {
    uint32_t hash = hashString(INIT_HASH, chars, length);

    return concatStrings(gc, strings, chars, length, hash, "", 0);
}

ObjUpvalue* newUpvalue(GC* gc, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(gc, ObjUpvalue, OBJ_UPVALUE);
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
        case 'e': return '\e';
        case '"': return '\"';
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
            unsigned char* setStr = setToString(AS_SET(value));
            printf("%s", setStr);
            free(setStr);
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