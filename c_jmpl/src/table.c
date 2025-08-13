#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "obj_string.h"
#include "value.h"
#include "gc.h"

// NOTE: should be fine-tuned once table implemented and tested
#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(GC* gc, Table* table) {
    FREE_ARRAY(gc, Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    // Map the key's hash code to an index in the array
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = NULL;

    while (true) {
        Entry* entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NULL(entry->value)) {
                // Empty entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                // Found a tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            // Found a key
            return entry;
        }

        if (entry->key == key || entry->key == NULL) {
            return entry;
        }

        // Collision, so start linear probing
        index = (index + 1) & (capacity - 1);
    }
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry (table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(GC* gc, Table* table, int capacity) {
    Entry* entries = ALLOCATE(gc, Entry, capacity);

    // Initialise every element to be an empty bucket
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    // Insert entries into array
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if(entry->key == NULL) continue;

        Entry* destination = findEntry(entries, capacity, entry->key);
        destination->key = entry->key;
        destination->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(gc, Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(GC* gc, Table* table, ObjString* key, Value value) {
    // Grow the array when load factor reaches TABLE_MAX_LOAD
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(gc, table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NULL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;

    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    // Find the entry
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry
    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

void tableAddAll(GC* gc, Table* from, Table* to) {
    // Copy all entries of a table into another
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];

        if (entry->key != NULL) {
            tableSet(gc, to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const unsigned char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;
    uint32_t index = hash & (table->capacity - 1);

    while (true) {
        assert(index < table->capacity);
        Entry* entry = &table->entries[index];

        if (entry->key == NULL) {
            // Stop when an empty non-tombstone entry is found
            if (IS_NULL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->utf8, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(GC* gc, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject(gc, (Obj*)entry->key);
        markValue(gc, entry->value);
    }
}

/**
 * @brief Prints a diagnostic of the current table stats.
 * 
 * @param table A pointer to the table to run the diagnostic on
 */
void tableDebugStats(Table* table) {
    printf("------- Table Debug -------\n");
    printf("Capacity: %d\n", table->capacity);
    printf("Count: %d\n", table->count);
    printf("Load: %.2f\n", (float)table->count / (float)table->capacity);
    
    int tombstones = 0;
    int longestProbe = 0;

    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];

        if (entry->key == NULL) {
            if (!IS_NULL(entry->value)) tombstones++;
            continue;
        }

        // How far is the entry from its ideal spot
        uint32_t ideal = entry->key->hash % table->capacity;
        int dist = (i + table->capacity - ideal) % table->capacity;
        if (dist > longestProbe) longestProbe = dist;
    }
    
    printf("Tombstones: %d\n", tombstones);
    printf("Longest probe distance: %d\n", longestProbe);
    printf("---------------------------\n");
}