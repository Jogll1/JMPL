#ifndef c_jmpl_unicode
#define c_jmpl_unicode

#include <stdint.h>

#define UNICODE_MAX (0x10FFFF)

uint32_t utf8ToUnicode(const unsigned char* input, int numBytes);
size_t unicodeToUtf8(uint32_t codePoint, unsigned char* output);
size_t getCharByteCount(unsigned char byte);

#endif