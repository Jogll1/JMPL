#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "obj_string.h"
#include "memory.h"
#include "object.h"
#include "unicode.h"
#include "gc.h"
#include "vm.h"

#define FNV_INIT_HASH 2166136261u
#define FNV_PRIME     16777619

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
 * @brief Hashes a char array using the FNV-1a hashing algorithm.
 * 
 * @param key    The char array that makes up the string
 * @param length The length of the string
 * @return       A hashed form of the string
 */
static uint32_t hashString(const unsigned char* key, int length) {
    uint32_t hash = FNV_INIT_HASH;

    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

// ======================================================================
// =================        Unicode String              =================
// ======================================================================

// static uint32_t hashUCS1(UCS1* codePoints, size_t length) {
//     uint32_t hash = FNV_INIT_HASH;

//     for (int i = 0; i < length; i++) {
//         hash ^= (uint8_t)codePoints[i];
//         hash *= FNV_PRIME;
//     }

//     return hash;
// }

// static uint32_t hashUCS2(UCS2* codePoints, size_t length) {
//     uint32_t hash = FNV_INIT_HASH;

//     for (int i = 0; i < length; i++) {
//         UCS2 c = codePoints[i];
//         hash ^= (uint8_t)(c & 0xFF);
//         hash *= FNV_PRIME;
//         hash ^= (uint8_t)(c >> 8);
//         hash *= FNV_PRIME;
//     }

//     return hash;
// }

// static uint32_t hashUCS4(UCS4* codePoints, size_t length) {
//     uint32_t hash = FNV_INIT_HASH;

//     for (int i = 0; i < length; i++) {
//         UCS4 c = codePoints[i];
//         hash ^= (uint8_t)(c & 0xFF);
//         hash *= FNV_PRIME;
//         hash ^= (uint8_t)((c >> 8) & 0xFF);
//         hash *= FNV_PRIME;
//         hash ^= (uint8_t)((c >> 16) & 0xFF);
//         hash *= FNV_PRIME;
//         hash ^= (uint8_t)((c >> 24) & 0xFF);
//         hash *= FNV_PRIME;
//     }

//     return hash;
// }

// /**
//  * @brief Hashes a unicode code point array using the FNV-1a hashing algorithm.
//  * 
//  * @param string The string to hash
//  * @return       A hashed form of the string
//  */
// static uint32_t hashUnicodeString(StringKind kind, void* codePoints, size_t length) {
//     uint32_t hash = FNV_INIT_HASH;

//     switch(kind) {
//         case KIND_1_BYTE: {
//             return hashUCS1(codePoints, length);
//         }
//         case KIND_2_BYTE: {
//             return hashUCS2(codePoints, length);
//         }
//         case KIND_4_BYTE: {
//             return hashUCS4(codePoints, length);
//         }
//     }
// }

static ObjUnicodeString* allocateUnicodeString(GC* gc, StringKind kind, void* codePoints, size_t length, unsigned char* utf8, size_t utf8Length, uint32_t hash) {
    ObjUnicodeString* string = ALLOCATE_OBJ(gc, ObjUnicodeString, OBJ_STRING_UNICODE);
    string->kind = kind;
    string->length = length;

    // IF ASCII -> no need for utf8 ?
    
    switch(kind) {
        case KIND_1_BYTE: {
            string->as.ucs1 = codePoints;

            string->utf8 = NULL;
            break;
        }
        case KIND_2_BYTE: {
            string->as.ucs2 = codePoints;
            break;
        }
        case KIND_4_BYTE: {
            string->as.ucs4 = codePoints;
            break;
        }
    }

    string->hash = hash;

    // pushTemp(gc, OBJ_VAL(string));
    // tableSet(gc, &vm.strings, string, NULL_VAL);
    // popTemp(gc);

    return string;
}

/**
 * @brief Takes in an array of UTF-8 encoded bytes and returns a ObjUnicodeString pointer.
 * 
 * @param gc     The garbage collector
 * @param chars  A UTF-8 byte sequence
 * @param length The size of the byte sequence
 * 
 * Strings are interned and hashed by their UTF-8 sequence.
 */
ObjUnicodeString* copyUnicodeString(GC* gc, const unsigned char* utf8Bytes, int utf8Length) {
    uint32_t hash = hashString(utf8Bytes, utf8Length);

    // ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    // if(interned != NULL) return interned;

    // Convert the utf8 byte sequence into code points


// #define COPY_STRING(type) \
//     do { \
//         heapCodePoints = ALLOCATE(gc, type, length + 1); \
//         memcpy(heapCodePoints, codePoints, length); \
//         ((type*)heapCodePoints)[length] = '\0'; \
//     } while (false)

//     void* heapCodePoints = NULL;
//     switch (kind) {
//         case KIND_1_BYTE: COPY_STRING(UCS1); break;
//         case KIND_2_BYTE: COPY_STRING(UCS2); break;
//         case KIND_4_BYTE: COPY_STRING(UCS4); break;
//     }
// #undef COPY_STRING

//     return allocateUnicodeString(gc, kind, heapCodePoints, length, hash);
}

// ======================================================================
// =================        ASCII String                =================
// ======================================================================

static ObjString* allocateString(GC* gc, unsigned char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(gc, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    pushTemp(gc, OBJ_VAL(string));
    tableSet(gc, &vm.strings, string, NULL_VAL);
    popTemp(gc);

    return string;
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
 * @brief Concatenate strings a and b if either are a string.
 * 
 * @param a Value a
 * @param b Value b
 * @return  A pointer to the concatenated string
 */
ObjString* concatenateString(GC* gc, Value a, Value b) {
    pushTemp(gc, b);
    pushTemp(gc, a);

    unsigned char* aStr = valueToString(a);
    unsigned char* bStr = valueToString(b);
    int aLen = strlen(aStr);
    int bLen = strlen(bStr);

    int length = aLen + bLen;
    unsigned char* chars = ALLOCATE(gc, unsigned char, length + 1);

    memcpy(chars, aStr, aLen);
    memcpy(chars + aLen, bStr, bLen);
    chars[length] = '\0'; // Null terminator

    free(aStr);
    free(bStr);
    popTemp(gc);
    popTemp(gc);

    ObjString* result = takeString(gc, chars, length);
    return result;
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