#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "unicode.h"

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
size_t getCharByteCount(unsigned char byte) {
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
 * @brief Converts a Unicode code point to its UTF-8 encoding.
 * 
 * @param codePoint The Unicode code point of a character
 * @param output    A char pointer (string) for the output
 * @return          The number of bytes 
 */
size_t unicodeToUtf8(uint32_t codePoint, unsigned char* output) {
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