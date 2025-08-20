#ifndef c_jmpl_unicode_h
#define c_jmpl_unicode_h

#include "common.h"

#define ASCII_MAX (0x007F)
#define UTF8_2_BYTE_MAX (0x07FF)
#define UTF8_3_BYTE_MAX (0xFFFF)

#define UNICODE_MAX (0x10FFFF)

uint32_t utf8ToUnicode(const unsigned char* input, int numBytes);
int8_t unicodeToUtf8(uint32_t codePoint, unsigned char* output);
int8_t getCharByteCount(unsigned char byte);
int8_t getCodePointByteCount(uint32_t codePoint);

#endif