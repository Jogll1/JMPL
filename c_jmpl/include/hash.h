#ifndef c_jmpl_hash_h
#define c_jmpl_hash_h

#include "common.h"
#include "value.h"
#include "set.h"
#include "tuple.h"

// #define FNV_INIT_HASH 2166136261u
// #define FNV_PRIME     16777619
#define FNV_INIT_HASH ((hash_t)0xCBF29CE484222325ULL)
#define FNV_PRIME     ((hash_t)0x00000100000001B3ULL)

typedef uint64_t hash_t;

hash_t hashString(hash_t hash, const unsigned char* key, int length);
hash_t hashValue(Value value);

#endif