#ifndef c_jmpl_valtable_h
#define c_jmpl_valtable_h

#include "common.h"
#include "value.h"

typedef struct {
    Value key;
    Value value; // Dummy
} ValEntry;

typedef struct {
    int count;
    int capacity;
    ValEntry* entries;
} ValTable;

uint32_t hashValue(Value value);

void initValTable(ValTable* table);
void freeValTable(ValTable* table);

bool valTableGet(ValTable* table, Value key, Value* value);
bool valTableSet(ValTable* table, Value key, Value value);
bool valTableDelete(ValTable* table, Value key);
void valTableAddAll(ValTable* from, ValTable* to);

#endif