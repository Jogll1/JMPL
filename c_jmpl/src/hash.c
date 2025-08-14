#include <assert.h>

#include "hash.h"

#include "value.h"
#include "obj_string.h"
#include "set.h"
#include "tuple.h"

#define TRUE_HASH  0xAAAA
#define FALSE_HASH 0xBBBB
#define NULL_HASH  0xCCCC

/**
 * @brief Hashes a char array using the FNV-1a hashing algorithm.
 * 
 * @param hash   An initial hash
 * @param key    The char array that makes up the string
 * @param length The length of the string
 * @return       A hashed form of the string
 */
uint64_t hashString(uint64_t hash, const unsigned char* key, int length) {
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

/**
 * @brief Hashes a set using the FNV-1a hashing algorithm.
 * 
 * @param set The set to hash
 * @return    A hashed form of the set
 */
static uint64_t hashSet(ObjSet* set) {
    uint64_t hash = FNV_INIT_HASH;

    for (int i = 0; i < set->capacity; i++) {
        Value element = set->elements[i];
        if (!IS_NULL(element)) {
            uint64_t elemHash = hashValue(element);

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
static uint64_t hashTuple(ObjTuple* tuple) {
    uint64_t hash = FNV_INIT_HASH;

    for (int i = 0; i < tuple->size; i++) {
        Value value = tuple->elements[i];

        uint64_t elemHash = hashValue(value);

        hash ^= elemHash;
        hash *= FNV_PRIME;
    }
}

static uint64_t hashObject(Obj* obj) {
    switch(obj->type) {
        case OBJ_SET:    return hashSet((ObjSet*)(obj));
        case OBJ_TUPLE:  return hashTuple((ObjTuple*)(obj));
        case OBJ_STRING: {
            ObjString* string = (ObjString*)(obj);
            return hashString(FNV_INIT_HASH, string->utf8, string->utf8Length);
        }
        default:         return (uint64_t)((uintptr_t)obj >> 2);
    }
}

uint64_t hashValue(Value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        return AS_BOOL(value) ? TRUE_HASH : FALSE_HASH;
    } else if (IS_NULL(value)) {
        return NULL_HASH;
    } else if (IS_NUMBER(value)) {
        uint64_t bits = (uint64_t)AS_NUMBER(value);
        return (uint64_t)(bits ^ (bits >> 32));
    } else if (IS_CHAR(value)) {
        return AS_CHAR(value);
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
            return (uint64_t)(bits ^ (bits >> 32));
        }
        case VAL_CHAR: {
            return *(uint64_t*)&value.as.character;
        }
        case VAL_OBJ: {
            return hashObject(AS_OBJ(value));
        }
        default: return 0;
    }
#endif
}