#ifndef c_jmpl_table_h
#define c_jmpl_table_h

#include "value.h"
#include "hash.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(GC* gc, Table* table);

bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(GC* gc, Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(GC* gc, Table* from, Table* to);

ObjString* tableFindString(GC* gc, Table* table, const unsigned char* chars, int length, hash_t hash);
Entry* tableFindJoinedStrings(GC* gc, Table* table, const unsigned char* a, int aLen, const unsigned char* b, int bLen, hash_t hash);
void tableRemoveWhite(Table* table);
void markTable(GC* gc, Table* table);
void tableDebugStats(Table* table);

#endif