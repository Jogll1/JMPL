#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "obj_string.h"
#include "memory.h"
#include "object.h"
#include "gc.h"
#include "vm.h"

static ObjString* allocateString(GC* gc, unsigned char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(gc, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string));
    tableSet(gc, &vm.strings, string, NULL_VAL);
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

ObjString* takeString(GC* gc, unsigned char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) {
        FREE_ARRAY(gc, char, chars, length + 1);
        return interned;
    }

    return allocateString(gc, chars, length, hash);
}

ObjString* copyString(GC* gc, const unsigned char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) return interned;

    unsigned char* heapChars = ALLOCATE(gc, char, length + 1);
    memcpy(heapChars, chars, length);

    heapChars[length] = '\0';

    return allocateString(gc, heapChars, length, hash);
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