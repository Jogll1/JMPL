#ifndef c_jmpl_hash_h
#define c_jmpl_hash_h

#include "common.h"
#include "value.h"
#include "set.h"
#include "tuple.h"

// #define FNV_INIT_HASH 2166136261u
// #define FNV_PRIME     16777619
#define FNV_INIT_HASH 0xCBF29CE484222325
#define FNV_PRIME     0x00000100000001B3

uint64_t hashString(uint64_t hash, const unsigned char* key, int length);
uint64_t hashValue(Value value);

#endif