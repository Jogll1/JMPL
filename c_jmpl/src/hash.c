#include <assert.h>

#include "hash.h"

#include "value.h"
#include "obj_string.h"
#include "set.h"
#include "tuple.h"
#include "../lib/xxhash/xxhash.h"

#define TRUE_HASH  0xAAAA
#define FALSE_HASH 0xBBBB
#define NULL_HASH  0xCCCC

static hash_t hashAvalanche(hash_t hash) {
    hash ^= hash >> 33;
    hash *= 1610612741;
    hash ^= hash >> 29;
    hash *= 805306457;
    hash ^= hash >> 32;

    return hash;
}

/**
 * @brief Hashes a char array using the FNV-1a hashing algorithm.
 * 
 * @param hash   An initial hash
 * @param key    The char array that makes up the string
 * @param length The length of the string
 * @return       A hashed form of the string
 */
hash_t hashString(hash_t hash, const unsigned char* key, int length) {
    // for (int i = 0; i < length; i++) {
    //     hash ^= key[i];
    //     hash *= FNV_PRIME;
    // }

    // return hashAvalanche(hash);

    return (hash_t)XXH64(key, length, hash);
}

/**
 * @brief Hashes a set using the FNV-1a hashing algorithm.
 * 
 * @param set The set to hash
 * @return    A hashed form of the set
 */
static hash_t hashSet(ObjSet* set) {
    hash_t hash = FNV_INIT_HASH;

    for (int i = 0; i < set->capacity; i++) {
        Value element = set->elements[i];
        if (!IS_NULL(element)) {
            hash_t elemHash = hashValue(element);

            hash ^= elemHash;
            hash *= FNV_PRIME;
        }
    }
}


/**
 * @brief Hashes a tuple using the FNV-1a hashing algorithm.
 * 
 * @param tuple The tuple to hash
 * @return      A hashed form of the tuple
 */
static hash_t hashTuple(ObjTuple* tuple) {
    hash_t hash = FNV_INIT_HASH;

    for (int i = 0; i < tuple->size; i++) {
        Value value = tuple->elements[i];

        hash_t elemHash = hashValue(value);

        hash ^= elemHash;
        hash *= FNV_PRIME;
    }
}

static hash_t hashObject(Obj* obj) {
    switch(obj->type) {
        case OBJ_SET:    return hashSet((ObjSet*)(obj));
        case OBJ_TUPLE:  return hashTuple((ObjTuple*)(obj));
        case OBJ_STRING: {
            ObjString* string = (ObjString*)(obj);
            return (hash_t)hashString(FNV_INIT_HASH, string->utf8, string->utf8Length);
        }
        default:         return (hash_t)((uintptr_t)obj >> 2);
    }
}

hash_t hashValue(Value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        return AS_BOOL(value) ? TRUE_HASH : FALSE_HASH;
    } else if (IS_NULL(value)) {
        return NULL_HASH;
    } else if (IS_NUMBER(value)) {
        uint64_t bits = (uint64_t)AS_NUMBER(value);
        return (hash_t)(bits ^ (bits >> 32));
    } else if (IS_CHAR(value)) {
        return (hash_t)(AS_CHAR(value));
    } else if (IS_OBJ(value)) {
        return hashObject(AS_OBJ(value));
    } 
    return 0;
#else
    switch(value.type) {
        case VAL_BOOL: return AS_BOOL(value) ? TRUE_HASH : FALSE_HASH;
        case VAL_NULL: return NULL_HASH;
        case VAL_NUMBER: {
            uint64_t bits = *(uint64_t*)&value.as.number;
            return (hash_t)(bits ^ (bits >> 32));
        }
        case VAL_CHAR: {
            return *(hash_t*)&value.as.character;
        }
        case VAL_OBJ: {
            return hashObject(AS_OBJ(value));
        }
        default: return 0;
    }
#endif
}