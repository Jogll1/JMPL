#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "obj_string.h"
#include "memory.h"
#include "object.h"
#include "unicode.h"
#include "gc.h"
#include "vm.h"
#include "hash.h"

// ===================================
// ======== Escape Characters ========
// ===================================

bool isHex(unsigned char c) {
    return c >= '0' && c <= '9' ||
           c >= 'a' && c <= 'f' ||
           c >= 'A' && c <= 'F';
}

int hexToValue(unsigned char c) {
    assert(isHex(c));
    if (c >= '0' && c <= '9') return (unsigned char)(c - '0');
    if (c >= 'A' && c <= 'F') return (unsigned char)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (unsigned char)(c - 'a' + 10);
    return 0;
}

/**
 * @brief Returns the corresponding escaped character given a character.
 * 
 * @param esc The character to escape
 * @return    The escaped character
 */
unsigned char decodeSimpleEscape(unsigned char esc) {
    switch (esc) {
        case 'a':  return '\a';
        case 'b':  return '\b';
        case 'e':  return '\e';
        case 'f':  return '\f';
        case 'n':  return '\n';
        case 'r':  return '\r';
        case 't':  return '\t';
        case 'v':  return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '\"': return '\"';
        default:   return esc;
    }
}

EscapeType getEscapeType(unsigned char esc) {
    switch (esc) {
        case 'a':
        case 'b':
        case 'e':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
        case '\\':
        case '\'':
        case '\0':
        case '"': return ESC_SIMPLE;
        case 'x': return ESC_HEX;
        case 'u': return ESC_UNICODE;
        case 'U': return ESC_UNICODE_LG;
        default:  return ESC_INVALID;
    }
}

// ===================================
// ========      Strings      ========
// ===================================

void freeString(GC* gc, ObjString* string) {
    FREE_ARRAY(gc, char, string->utf8, string->utf8Length + 1);
            
    switch (string->kind) {
        case KIND_1_BYTE: FREE_ARRAY(gc, UCS1, string->as.ucs1, string->length); break;
        case KIND_2_BYTE: FREE_ARRAY(gc, UCS2, string->as.ucs2, string->length); break;
        case KIND_4_BYTE: FREE_ARRAY(gc, UCS4, string->as.ucs4, string->length); break;
    }

    FREE(gc, ObjString, string);
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
        unsigned char leadingByte = utf8[i];

        if (leadingByte < 0x80) {
            // 1 byte in UTF-8, 1 byte code point
            // U+0000 - U+007F
            kind = KIND_ASCII;
            i += 1;
        } else if (leadingByte < 0xC4) {
            // 2 bytes in UTF-8, 1 byte code point
            // U+0080 - U+00FF
            kind = KIND_1_BYTE;
            i += 2;
        } else if (leadingByte < 0xE0) {
            // 2 bytes in UTF-8, 2 byte code point
            // U+0100 - U+07FF
            kind = KIND_2_BYTE;
            i += 2;
        } else if (leadingByte <= 0xEF) {
            // 3 bytes in UTF-8, 2 byte code point
            // U+0800 - U+FFFF
            kind = KIND_2_BYTE;
            i += 3;
        } else {
            // 4 bytes in UTF-8, 4 byte code point
            // U+100000 - U+10FFFF
            return KIND_4_BYTE;
        }
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

static ObjString* allocateString(GC* gc, StringKind kind, const void* codePoints, size_t length, unsigned char* utf8, size_t utf8Length, hash_t hash) {
    assert(codePoints != NULL);

    ObjString* string = ALLOCATE_OBJ(gc, ObjString, OBJ_STRING);
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
 * @brief Takes in an array of UTF-8 encoded bytes and returns a ObjString pointer.
 * 
 * @param gc         The garbage collector
 * @param utf8       A UTF-8 byte sequence
 * @param utf8Length The size of the byte sequence
 * 
 * Strings are interned and hashed by their UTF-8 sequence.
 */
ObjString* copyString(GC* gc, const unsigned char* utf8, int utf8Length) {
    hash_t hash = hashString(FNV_INIT_HASH, utf8, utf8Length);
    
    ObjString* interned = tableFindString(gc, &vm.strings, utf8, utf8Length, hash);
    if (interned != NULL) {
        return interned;
    }

    // Store the UTF-8 encoded string (null-terminated)
    unsigned char* heapUtf8 = ALLOCATE(gc, unsigned char, utf8Length + 1);
    memcpy(heapUtf8, utf8, utf8Length);
    heapUtf8[utf8Length] = '\0';

    // Create the code point array
    StringKind kind = getUtf8StringKind(utf8, utf8Length);
    const void* heapCodePoints = NULL;
    size_t length = createCodePointArray(gc, kind, &heapCodePoints, utf8, utf8Length);

    return allocateString(gc, kind, heapCodePoints, length, heapUtf8, utf8Length, hash);
}

/**
 * @brief Concatenates two strings.
 * 
 * @param a      A string
 * @param b      A string
 * @return       A pointer to the concatenated string
 */
static ObjString* concatenateStrings(GC* gc, ObjString* a, ObjString* b) {
    hash_t hash = hashString(a->hash, b->utf8, b->utf8Length);
    Entry* entry = tableFindJoinedStrings(gc, &vm.strings, a->utf8, a->utf8Length, b->utf8, b->utf8Length, hash);
    if (entry->key != NULL) {
        return entry->key;
    }

    // Concat utf8
    int utf8Length = a->utf8Length + b->utf8Length;
    unsigned char* utf8 = ALLOCATE(gc, unsigned char, utf8Length + 1);

    memcpy(utf8, a->utf8, a->utf8Length);
    memcpy(utf8 + a->utf8Length, b->utf8,  b->utf8Length);
    utf8[utf8Length] = '\0'; // Null terminator

    // Create the code point array
    StringKind kind = b->kind > a->kind ? b->kind : a->kind;
    const void* heapCodePoints = NULL;
    size_t length = createCodePointArray(gc, kind, &heapCodePoints, utf8, utf8Length);

    return allocateString(gc, kind, heapCodePoints, length, utf8, utf8Length, hash);
}

/**
 * @brief Concatenate a string and a value.
 * 
 * @param a      A string
 * @param b      A value
 * @param aFirst If true, concat a + b, else b + a
 * @return       A pointer to the concatenated string
 */
static ObjString* concatenateStringAndValue(GC* gc, ObjString* a, Value b, bool aFirst) {
    unsigned char* bUtf8 = valueToString(b);
    int bUtf8Length = strlen(bUtf8);

    hash_t hash = hashString(a->hash, bUtf8, bUtf8Length);
    Entry* entry = tableFindJoinedStrings(gc, &vm.strings, a->utf8, a->utf8Length, bUtf8, bUtf8Length, hash);
    if (entry->key != NULL) {
        free(bUtf8);
        return entry->key;
    }

    // Concat utf8
    int utf8Length = a->utf8Length + bUtf8Length;
    unsigned char* utf8 = ALLOCATE(gc, unsigned char, utf8Length + 1);

    if (aFirst) {
        memcpy(utf8, a->utf8, a->utf8Length);
        memcpy(utf8 + a->utf8Length, bUtf8, bUtf8Length);
    } else {
        memcpy(utf8, bUtf8, bUtf8Length);
        memcpy(utf8 + bUtf8Length, a->utf8, a->utf8Length);
    }

    utf8[utf8Length] = '\0'; // Null terminator

    // Create the code point array
    StringKind bKind = getUtf8StringKind(bUtf8, bUtf8Length);
    StringKind kind = bKind > a->kind ? bKind : a->kind;
    const void* heapCodePoints = NULL;
    size_t length = createCodePointArray(gc, kind, &heapCodePoints, utf8, utf8Length);

    free(bUtf8);
    return allocateString(gc, kind, heapCodePoints, length, utf8, utf8Length, hash);
}

/**
 * @brief Concatenate strings a and b if either are a string.
 * 
 * @param a      Value a
 * @param b      Value b
 * @param aFirst If true, concat a + b, else b + a
 * @return       A pointer to the concatenated string
 */
ObjString* concatenateStringsHelper(GC* gc, Value a, Value b) {
    assert(IS_STRING(a) || IS_STRING(b));

    if (IS_STRING(a) && IS_STRING(b)) {
        return concatenateStrings(&vm.gc, AS_STRING(a), AS_STRING(b));
    } else if (IS_STRING(a)) {
        return concatenateStringAndValue(&vm.gc, AS_STRING(a), b, true);
    } else {
        return concatenateStringAndValue(&vm.gc, AS_STRING(b), a, false);
    }
}

/**
 * @brief Return the character at an index in a string.
 * 
 * @param string A string
 * @param index  An index
 */
Value indexString(ObjString* string, size_t index) {
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
 * @brief Print an ObjString to the console.
 * 
 * @param string A pointer to an ObjString
 */
void printJMPLString(ObjString* string) {
    printf("%s", string->utf8);
}