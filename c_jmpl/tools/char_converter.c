#include <stdio.h>

/**
 * @brief Converts a Unicode code point to its UTF-8 encoding.
 * 
 * @param codePoint The Unicode code point of a character
 * @param output     A char pointer (string) for the output
 */
void unicodeToUtf8(int codePoint, unsigned char* output) {
    if (codePoint <= 0x7F) {
        // 1-byte sequence
        output[0] = codePoint & 0x7F;
        output[1] = '\0';
    } else if (codePoint <= 0x7FF) {
        // 2-byte sequence
        output[0] = 0xC0 | ((codePoint >> 6) & 0x1F);
        output[1] = 0x80 | (codePoint & 0x3F);
        output[2] = '\0';
    } else if (codePoint <= 0xFFFF) {
        // 3-byte sequence
        output[0] = 0xE0 | ((codePoint >> 12) & 0x0F);
        output[1] = 0x80 | ((codePoint >> 6) & 0x3F);
        output[2] = 0x80 | (codePoint & 0x3F);
        output[3] = '\0';
    } else if (codePoint <= 0x10FFFF) {
        // 4-byte sequence
        output[0] = 0xF0 | ((codePoint >> 18) & 0x07);
        output[1] = 0x80 | ((codePoint >> 12) & 0x3F);
        output[2] = 0x80 | ((codePoint >> 6) & 0x3F);
        output[3] = 0x80 | (codePoint & 0x3F);
        output[4] = '\0';
    } else {
        // Invalid code point
        output[0] = '\0';
    }
}

int main() {
    int codePoint = 0x2227; // Unicode U+2227 (∧)
    unsigned char utf8[5];  // Buffer to hold UTF-8 bytes (max 4 bytes + null-terminator)

    unicodeToUtf8(codePoint, utf8);

    printf("UTF-8 Encoding: ");
    for (int i = 0; utf8[i] != '\0'; i++) {
        printf("%02X ", utf8[i]);
    }
    printf("\n");

    return 0;
}