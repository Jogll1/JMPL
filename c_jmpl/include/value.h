#ifndef c_jmpl_value_h
#define c_jmpl_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

/**
 * @brief The type of a Value.
 */
typedef enum {
    VAL_BOOL,   /**> Boolean */
    VAL_NULL,   /**> Null */
    VAL_NUMBER, /**> Number */
    VAL_OBJ     /**> Object */
} ValueType;

/**
 * @brief A JMPL representation of a value.
 */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Type check

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NULL(value)    ((value).type == VAL_NULL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

#define IS_INTEGER(value) (IS_NUMBER(value) && (int)(AS_NUMBER(value)) == AS_NUMBER(value))

// JMPL Value to C value

#define AS_OBJ(value)     ((value).as.obj)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

// C value to JMPL Value

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NULL_VAL          ((Value){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

// Convert values to strings

#define BOOL_TO_STRING(value) ((value) ? "true" : "false");
#define NULL_TO_STRING ("null")
#define NUMBER_TO_STRING(value, resultPtr) \
    do { \
        if((value) == (int)(value)) { \
            int size = snprintf(NULL, 0, "%d", (int)(value)) + 1; \
            *(resultPtr) = (unsigned char*)malloc(size); \
            snprintf(*(resultPtr), size, "%d", (int)(value)); \
        } else { \
            int size = snprintf(NULL, 0, "%g", (value)) + 1; \
            *(resultPtr) = (unsigned char*)malloc(size); \
            snprintf(*(resultPtr), size, "%g", (value)); \
        } \
    } while(false)

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif