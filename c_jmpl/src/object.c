#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = true;

    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjClosure* newClosure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for(int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(NativeFn function, int arity) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->arity = arity;
    native->function = function;
    return native;
}

static ObjString* allocateString(unsigned char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NULL_VAL);
    pop();

    return string;
}

/**
 * @brief Hashes a char array using the FNV-1a hashing algorithm.
 * 
 * @param key    The char array that makes up the string
 * @param length The length of the string
 * @return       A hashed form of the string
 */
static uint32_t hashString(const unsigned char* key, int length) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }

    return hash;
}

ObjString* takeString(unsigned char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* copyString(const unsigned char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) return interned;

    unsigned char* heapChars = ALLOCATE(char, length + 1);

    // Cppy each character to the new string - manually deal with escape characters
    int j = 0;
    for (int i = 0; i < length; i++)
    {
        if (chars[i] == '\\' && i + 1 < length) {
            i++;
            switch (chars[i])
            {
                case 'n': heapChars[j++] = '\n'; break;
                case 't': heapChars[j++] = '\t'; break;
                case '\\': heapChars[j++] = '\\'; break;
                case '"': heapChars[j++] = '"'; break;
                default: heapChars[j++] = chars[i]; break;
            }
        } else {
            heapChars[j++] = chars[i];
        }
    }

    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash);
}

ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static void printFunction(ObjFunction* function) {
    if(function->name == NULL) {
        printf("<script>");
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

/**
 * @brief Converts a value to an ObjString.
 * 
 * @param value the value to convert
 * @return      a pointer to an ObjString
 */
ObjString* valueToString(Value value) {
    if(IS_STRING(value)) {
        return AS_STRING(value);
    }

    if(IS_FUNCTION(value)) {
        unsigned char* str = AS_FUNCTION(value)->name->chars;
        return AS_STRING(OBJ_VAL(copyString(str, strlen(str))));
    }

    unsigned char* str;
    switch(value.type) {
        case VAL_BOOL:
            str = BOOL_TO_STRING(AS_BOOL(value));
            break;
        case VAL_NULL:
            str = NULL_TO_STRING;
            break;
        case VAL_NUMBER:
            NUMBER_TO_STRING(AS_NUMBER(value), &str);
            break;
        default: return NULL;
    }

    ObjString* result = AS_STRING(OBJ_VAL(copyString(str, strlen(str))));

    return result;
}

void printObject(Value value) {
    switch(OBJ_TYPE(value)) {
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native %s>", AS_FUNCTION(value)->name->chars);
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
        default: return;
    }
}