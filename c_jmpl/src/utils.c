#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "utils.h"

#pragma region IO

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define DIR_SEP1 '\\'
    #define DIR_SEP2 '/'
    #define PORTABLE_REAL_PATH(path, resolved) (_access(_fullpath(resolved, path, MAX_PATH_SIZE), 0) == 0)
#else
    #include <unistd.h>
    #define DIR_SEP1 '/'
    #define DIR_SEP2 '/'
    #define PORTABLE_REAL_PATH(path, resolved) (realpath(path, resolved) != NULL)
#endif

bool getAbsolutePath(const char* path, char* resolved) {
    return PORTABLE_REAL_PATH(path, resolved);
}

void getFileName(const char* path, char* name, size_t resolvedSize) {
    const char *base = path;
    const char *dot;

    // Find last path separator (works for both \ and / on Windows)
    const char *sep1 = strrchr(path, DIR_SEP1);
    const char *sep2 = strrchr(path, DIR_SEP2);
    if (sep1 && sep2) {
        base = (sep1 > sep2 ? sep1 : sep2) + 1;
    } else if (sep1 || sep2) {
        base = (sep1 ? sep1 : sep2) + 1;
    }
    
    // Find last dot after separator
    dot = strrchr(base, '.');
    if (!dot || dot == base) dot = base + strlen(base); // No extension

    size_t len = dot - base;
    if (len >= resolvedSize) len = resolvedSize - 1;

    strncpy(name, base, len);
    name[len] = '\0';
}

unsigned char* readFile(const char* path) {
    // Open file
    FILE* file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(IO_ERROR);
    }

    // Seek to the end and get how many bytes the file is
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    
    // Rewind file to the end
    rewind(file);

    // Allocate a string the size of the file
    unsigned char* buffer = (unsigned char*)malloc(fileSize + 1);

    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(IO_ERROR);
    }

    // Read the whole file
    size_t bytesRead = fread(buffer, sizeof(unsigned char), fileSize, file);

    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(IO_ERROR);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

#pragma endregion

#pragma region Hex

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

#pragma endregion

#pragma region Escape Sequences

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

#pragma endregion

#pragma region Unicode

enum {
    MASK_0F = 0x0F,
    MASK_03 = 0x03
};

/**
 * @brief Determine the length of a character in bytes that is encoded with UTF-8.
 * 
 * @param byte The first byte of the character
 * @returns    How long the character's byte sequence is
 */
int8_t getCharByteCount(unsigned char byte) {
    if (byte < 0x80) {
        return 1; // ASCII
    } else if ((byte & 0xE0) == 0xC0) {
        return 2;
    } else if ((byte & 0xF0) == 0xE0) {
        return 3;
    } else if ((byte & 0xF8) == 0xF0) {
        return 4;
    } else {
        return 0; // Invalid
    }
}

/**
 * @brief Determine the length of a code point in bytes when it is encoded with UTF-8.
 * 
 * @param codePoint A code point
 * @returns         How long the character's byte sequence will be when encoded in UTF-8
 */
int8_t getCodePointByteCount(uint32_t codePoint) {
    if (codePoint < 0x80) {
        // 1 byte in UTF-8
        // U+0000 - U+007F
        return 1;
    } else if (codePoint < 0x800) {
        // 2 bytes in UTF-8
        // U+0080 - U+07FF
        return 2;
    } else if (codePoint < 0x100000) {
        // 3 bytes in UTF-8
        // U+0800 - U+FFFF
        return 3;
    } else {
        // 4 bytes in UTF-8
        // U+100000 - U+10FFFF
        return 4;
    }
}

/**
 * @brief Converts a Unicode code point to its UTF-8 encoding.
 * 
 * @param codePoint The Unicode code point of a character
 * @param output    A char pointer (string) for the output
 * @return          The number of bytes 
 */
int8_t unicodeToUtf8(uint32_t codePoint, unsigned char* output) {
    codePoint = (uint64_t)codePoint;
    if (codePoint <= ASCII_MAX) {
        output[0] = codePoint & ASCII_MAX;
        output[1] = '\0';
        return 1;
    } else if (codePoint <= UTF8_2_BYTE_MAX) {
        output[0] = 0xC0 | ((codePoint >> 6) & 0x1F);
        output[1] = 0x80 | (codePoint & 0x3F);
        output[2] = '\0';
        return 2;
    } else if (codePoint <= UTF8_3_BYTE_MAX) {
        output[0] = 0xE0 | ((codePoint >> 12) & 0x0F);
        output[1] = 0x80 | ((codePoint >> 6) & 0x3F);
        output[2] = 0x80 | (codePoint & 0x3F);
        output[3] = '\0';
        return 3;
    } else if (codePoint <= UNICODE_MAX) {
        output[0] = 0xF0 | ((codePoint >> 18) & 0x07);
        output[1] = 0x80 | ((codePoint >> 12) & 0x3F);
        output[2] = 0x80 | ((codePoint >> 6) & 0x3F);
        output[3] = 0x80 | (codePoint & 0x3F);
        output[4] = '\0';
        return 4;
    } else {
        // Invalid code point
        output['\0'];
        return 0;
    }
}

uint32_t utf8ToUnicode(const unsigned char* input, int numBytes) {
    assert(numBytes > 0 && numBytes <= 4);

    if (numBytes == 1) {
        return (uint32_t)(*input);
    }

    size_t b4 = numBytes - 1;
    size_t b3 = numBytes - 2;
    uint8_t zzzz = input[b4] & MASK_0F;
    uint8_t yyyy = ((input[b3] & MASK_03) << 2) | ((input[b4] >> 4) & MASK_03);
    uint8_t xxxx = (input[b3] >> 2) & MASK_0F;

    if (numBytes > 2) {
        size_t b2 = numBytes - 3;
        uint8_t wwww = input[b2] & MASK_0F;

        if (numBytes > 3) {
            size_t b1 = numBytes - 4;
            uint8_t vvvv = ((input[b1] & MASK_03) << 2) | ((input[b2] >> 4) & MASK_03);
            uint8_t u    = (input[b1] >> 2) & MASK_03;

            return u << 20 | (vvvv << 16) | (wwww << 12) | (xxxx << 8) | (yyyy << 4) | zzzz;
        }

        return (wwww << 12) | (xxxx << 8) | (yyyy << 4) | zzzz;
    }

    return (xxxx << 8) | (yyyy << 4) | zzzz;
}

#pragma endregion