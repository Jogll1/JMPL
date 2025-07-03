#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"
#include "valtable.h"
#include "set.h"
#include "tuple.h"
#include "value.h"

// NOTE: should be fine-tuned once table implemented and tested
#define VAL_TABLE_MAX_LOAD 0.75

uint32_t hashValue(Value value) {
    switch(value.type) {
        case VAL_BOOL: return AS_BOOL(value) ? 0xAAAA : 0xBBBB;
        case VAL_NULL: return 0xCCCC;
        case VAL_NUMBER: {
            uint64_t bits = *(uint64_t*)&value.as.number;
            return (uint32_t)(bits ^ (bits >> 32));
        }
        case VAL_OBJ:  
            switch(AS_OBJ(value)->type) {
                case OBJ_SET:   return hashSet(AS_SET(value));
                case OBJ_TUPLE: return hashTuple(AS_TUPLE(value));
                default:        return (uint32_t)((uintptr_t)AS_OBJ(value) >> 2);
            }
        default:       return 0;
    }
}

void initValTable(ValTable* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeValTable(ValTable* table) {
    FREE_ARRAY(ValEntry, table->entries, table->capacity);
    initValTable(table);
}

static ValEntry* findEntry(ValEntry* entries, int capacity, Value key) {
    // Map the key's hash code to an index in the array
    uint32_t index = hashValue(key) % capacity;
    ValEntry* tombstone = NULL;

    while (true) {
        ValEntry* entry = &entries[index];

        if (IS_NULL(entry->key)) {
            if (IS_NULL(entry->value)) {
                // Empty entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                // Found a tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (valuesEqual(entry->key, key)) {
            // Found a key
            return entry;
        }

        // Collision, so start linear probing
        index = (index + 1) % capacity;
    }
}

bool valTableGet(ValTable* table, Value key, Value* value) {
    if(table->count == 0) return false;

    ValEntry* entry = findEntry(table->entries, table->capacity, key);
    if(IS_NULL(entry->key)) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(ValTable* table, int capacity) {
    ValEntry* entries = ALLOCATE(ValEntry, capacity);

    // Initialise every element to be an empty bucket
    for(int i = 0; i < capacity; i++) {
        entries[i].key = NULL_VAL;
        entries[i].value = NULL_VAL;
    }

    // Insert entries into array
    table->count = 0;
    for(int i = 0; i < table->capacity; i++) {
        ValEntry* entry = &table->entries[i];
        if(entry->key.type == VAL_NULL) continue;

        ValEntry* destination = findEntry(entries, capacity, entry->key);
        destination->key = entry->key;
        destination->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(ValEntry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool valTableSet(ValTable* table, Value key, Value value) {
    // Grow the array when load factor reaches TABLE_MAX_LOAD
    if(table->count + 1 > table->capacity * VAL_TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    ValEntry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = IS_NULL(entry->key);
    if(isNewKey && IS_NULL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;

    return isNewKey;
}

bool valTableDelete(ValTable* table, Value key) {
    if(table->count == 0) return false;

    // Find the entry
    ValEntry* entry = findEntry(table->entries, table->capacity, key);
    if(IS_NULL(entry->key)) return false;

    // Place a tombstone in the entry
    entry->key = NULL_VAL;
    entry->value = BOOL_VAL(true);

    return true;
}

void valTableAddAll(ValTable* from, ValTable* to) {
    // Copy all entries of a table into another
    for (int i = 0; i < from->capacity; i++) {
        ValEntry* entry = &from->entries[i];

        if(entry->key.type != VAL_NULL) {
            valTableSet(to, entry->key, entry->value);
        }
    }
}