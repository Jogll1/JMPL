#ifndef c_jmpl_valtable_h
#define c_jmpl_valtable_h

#include "common.h"
#include "value.h"
#include "gc.h"

typedef struct {
    Value key;
    Value value; // Dummy
} ValEntry;

typedef struct {
    int count;
    int capacity;
    ValEntry* entries;
} ValTable;

void initValTable(ValTable* table);
void freeValTable(GC* gc, ValTable* table);

bool valTableGet(ValTable* table, Value key, Value* value);
bool valTableSet(GC* gc, ValTable* table, Value key, Value value);
bool valTableDelete(ValTable* table, Value key);
void valTableAddAll(GC* gc, ValTable* from, ValTable* to);

#endif