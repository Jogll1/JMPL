#ifndef c_jmpl_table_h
#define c_jmpl_table_h

#include "value.h"

typedef struct {
    ObjUnicodeString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(GC* gc, Table* table);

bool tableGet(Table* table, ObjUnicodeString* key, Value* value);
bool tableSet(GC* gc, Table* table, ObjUnicodeString* key, Value value);
bool tableDelete(Table* table, ObjUnicodeString* key);
void tableAddAll(GC* gc, Table* from, Table* to);

ObjUnicodeString* tableFindString(Table* table, const unsigned char* chars, int length, uint32_t hash);
void tableRemoveWhite(Table* table);
void markTable(GC* gc, Table* table);
void tableDebugStats(Table* table);

#endif