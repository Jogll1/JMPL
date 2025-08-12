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

void freeUnicodeString(GC* gc, ObjUnicodeString* string) {
    FREE_ARRAY(gc, char, string->utf8, string->utf8Length + 1);
            
    switch (string->kind) {
        case KIND_1_BYTE: FREE_ARRAY(gc, UCS1, string->as.ucs1, string->length); break;
        case KIND_2_BYTE: FREE_ARRAY(gc, UCS2, string->as.ucs2, string->length); break;
        case KIND_4_BYTE: FREE_ARRAY(gc, UCS4, string->as.ucs4, string->length); break;
    }

    FREE(gc, ObjUnicodeString, string);
}

/**
 * @brief Get the kind of a UTF-8 character.
 * 
 * @param utf8       The leading byte of a UTF-8 character
 * @return           The kind of the character
 */
static StringKind getUtf8CharacterKind(const unsigned char leadingByte) {
    if (leadingByte < 0x80) {
        // 1 byte in UTF-8, 1 byte code point
        // U+0000 - U+007F
        return KIND_ASCII;
    } else if (leadingByte < 0xC4) {
        // 2 bytes in UTF-8, 1 byte code point
        // U+0080 - U+00FF
        return KIND_1_BYTE;
    } else if (leadingByte < 0xE0) {
        // 2 bytes in UTF-8, 2 byte code point
        // U+0100 - U+07FF
        return KIND_2_BYTE;
    } else if (leadingByte <= 0xEF) {
        // 3 bytes in UTF-8, 2 byte code point
        // U+0800 - U+FFFF
        return KIND_2_BYTE;
    } else {
        // 4 bytes in UTF-8, 4 byte code point
        // U+100000 - U+10FFFF
        return KIND_4_BYTE;
    }
}

static void printCodePoints(StringKind kind, const void* codePoints, size_t length) {
    printf("Output (%d): ", (int)length);
    for (size_t i = 0; i < length; i++) {
        switch (kind) {
            case KIND_ASCII: 
            case KIND_1_BYTE: printf("U+%04x ", ((UCS1*)codePoints)[i]); break;
            case KIND_2_BYTE: printf("U+%04x ", ((UCS2*)codePoints)[i]); break;
            case KIND_4_BYTE: printf("U+%06x ", ((UCS4*)codePoints)[i]); break;
        }
    }
    printf("\n");
}

/**
 * @brief Get the kind of a UTF-8 byte sequence.
 * 
 * @param utf8       A UTF-8 byte sequence (null-terminated)
 * @param utf8Length Length of the UTF-8 byte sequence
 * @return           The kind of the string
 */
static StringKind getUtf8StringKind(const unsigned char* utf8, size_t utf8Length) {
    StringKind kind = KIND_ASCII;

    size_t i = 0;
    while (i < utf8Length) {
        size_t numBytes = getCharByteCount(utf8[i]);
        StringKind charKind = getUtf8CharacterKind(utf8[i]);

        if (charKind == KIND_4_BYTE){
            return KIND_4_BYTE;
        } else if (charKind > kind) {
            kind = charKind;
        }
        
        i += numBytes;
    }
    
    return kind;
}

/**
 * @brief Convert an array of UTF-8 bytes to a UCS code point array.
 * 
 * @param output     An empty UCS array
 * @param kind       The target kind
 * @param utf8       A UTF-8 byte sequence (null-terminated)
 * @param utf8Length Length of the UTF-8 byte sequence
 * @return           The number of code points
 */
static size_t utf8ToUCS(void* output, StringKind kind, const unsigned char* utf8, size_t utf8Length) {
    assert(utf8 != NULL);

    size_t numPoints = 0; 
    size_t i = 0;
    while (i < utf8Length) {
        size_t numBytes = getCharByteCount(utf8[i]);
        uint32_t codePoint = utf8ToUnicode(&utf8[i], numBytes);

        if (kind <= KIND_1_BYTE) {
            ((UCS1*)output)[numPoints] = (UCS1)(codePoint & 0xFF);
        } else if (kind == KIND_2_BYTE) {
            ((UCS2*)output)[numPoints] = (UCS2)(codePoint & 0xFFFF);
        } else if (KIND_4_BYTE) {
            ((UCS4*)output)[numPoints] = codePoint;
        }

        numPoints++;
        i += numBytes;
    }

    return numPoints;
}

/**
 * @brief Convert an array of UTF-8 bytes to a UCS code point array (allocates).
 * 
 * @param gc         The garbage collector
 * @param kind       The kind of the string
 * @param output     An empty array of code points
 * @param utf8       A UTF-8 byte sequence
 * @param utf8Length The size of the byte sequence
 * @returns          The size of the code point list
 */
static size_t createCodePointArray(GC* gc, StringKind kind, const void** output, const unsigned char* utf8, int utf8Length) {
    size_t length = 0;

#define COPY_STRING(type) \
    do { \
        type* codePoints = ALLOCATE(gc, type, utf8Length); \
        length = utf8ToUCS(codePoints, kind, utf8, utf8Length); \
        *output = codePoints; \
    } while (false)

    switch (kind) {
        case KIND_ASCII: {
            *output = (void*)utf8;
            length = utf8Length;
            break;
        }
        case KIND_1_BYTE: COPY_STRING(UCS1); break;
        case KIND_2_BYTE: COPY_STRING(UCS2); break;
        case KIND_4_BYTE: COPY_STRING(UCS4); break;
    }
#undef COPY_STRING

    return length;
}

static ObjUnicodeString* allocateUnicodeString(GC* gc, StringKind kind, const void* codePoints, size_t length, unsigned char* utf8, size_t utf8Length, uint32_t hash) {
    assert(codePoints != NULL);

    ObjUnicodeString* string = ALLOCATE_OBJ(gc, ObjUnicodeString, OBJ_UNICODE_STRING);
    string->kind = kind;
    string->length = length;
    string->hash = hash;

    switch (kind) {
        case KIND_ASCII:
        case KIND_1_BYTE: string->as.ucs1 = (UCS1*)codePoints; break;
        case KIND_2_BYTE: string->as.ucs2 = (UCS2*)codePoints; break;
        case KIND_4_BYTE: string->as.ucs4 = (UCS4*)codePoints; break;
    }

    // Will be the same pointer for ascii
    string->utf8 = utf8;
    string->utf8Length = utf8Length;

    pushTemp(gc, OBJ_VAL(string));
    tableSet(gc, &vm.strings, string, NULL_VAL);
    popTemp(gc);

    return string;
}

/**
 * @brief Takes in an array of UTF-8 encoded bytes and returns a ObjUnicodeString pointer.
 * 
 * @param gc         The garbage collector
 * @param utf8       A UTF-8 byte sequence
 * @param utf8Length The size of the byte sequence
 * 
 * Strings are interned and hashed by their UTF-8 sequence.
 */
ObjUnicodeString* copyUnicodeString(GC* gc, const unsigned char* utf8, int utf8Length) {
    uint32_t hash = hashString(utf8, utf8Length);

    ObjUnicodeString* interned = tableFindString(&vm.strings, utf8, utf8Length, hash);
    if(interned != NULL) return interned;

    // Store the UTF-8 encoded string (null-terminated)
    unsigned char* heapUtf8 = ALLOCATE(gc, unsigned char, utf8Length + 1);
    memcpy(heapUtf8, utf8, utf8Length);
    heapUtf8[utf8Length] = '\0';

    // Create the code point array
    StringKind kind = getUtf8StringKind(utf8, utf8Length);
    const void* heapCodePoints = NULL;
    size_t length = createCodePointArray(gc, kind, &heapCodePoints, utf8, utf8Length);

    return allocateUnicodeString(gc, kind, heapCodePoints, length, heapUtf8, utf8Length, hash);
}

static ObjUnicodeString* takeUnicodeString(GC* gc, unsigned char* utf8, int utf8Length) {
    uint32_t hash = hashString(utf8, utf8Length);

    ObjUnicodeString* interned = tableFindString(&vm.strings, utf8, utf8Length, hash);
    if(interned != NULL) {
        FREE_ARRAY(gc, unsigned char, utf8, utf8Length + 1);
        return interned;
    }

    // Create the code point array
    StringKind kind = getUtf8StringKind(utf8, utf8Length);
    const void* heapCodePoints = NULL;
    size_t length = createCodePointArray(gc, kind, &heapCodePoints, utf8, utf8Length);

    return allocateUnicodeString(gc, kind, heapCodePoints, length, utf8, utf8Length, hash);
}

/**
 * @brief Concatenate strings a and b if either are a string.
 * 
 * @param a Value a
 * @param b Value b
 * @return  A pointer to the concatenated string
 */
ObjUnicodeString* concatenateUnicodeString(GC* gc, Value a, Value b) {
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

    ObjUnicodeString* result = takeUnicodeString(gc, chars, length);
    return result;
}

/**
 * @brief Return the character at an index in a string.
 * 
 * @param string A string
 * @param index  An index
 */
Value indexString(ObjUnicodeString* string, size_t index) {
    assert(index < string->length || index > 0);

    uint32_t codePoint;
    switch (string->kind) {
        case KIND_ASCII:
        case KIND_1_BYTE: codePoint = (uint32_t)string->as.ucs1[index]; break;
        case KIND_2_BYTE: codePoint = (uint32_t)string->as.ucs2[index]; break;
        case KIND_4_BYTE: codePoint = string->as.ucs4[index]; break;
    }

    return CHAR_VAL(codePoint);
}

/**
 * @brief Print an ObjUnicodeString to the console.
 * 
 * @param string A pointer to an ObjUnicodeString
 * 
 * Decodes each character in the string object, including escape characters.
 * For printing the raw characters, use printf().
 */
void printJMPLString(ObjUnicodeString* string) {
    // int length = string->length;
    // unsigned char* chars = string->utf8;

    // char* result = malloc(length + 1);
    // if (result == NULL) {
    //     fprintf(stderr, "Out of memory.\n");
    //     exit(1);
    // }
    // int ri = 0;

    // for (int i = 0; i < length; i++) {
    //     // Add each string and decode escapes if necessary
    //     if (chars[i] == '\\' && i + 1 < length) {
    //         result[ri++] = decodeEscape(chars[i + 1]);
    //         i++; // Skip the escaped character
    //     } else {
    //         result[ri++] = chars[i];
    //     }
    // }

    // result[ri] = '\0';
    
    // printf("%s", result);
    // free(result);

    printf("%s", string->utf8);
}