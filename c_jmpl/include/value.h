#ifndef c_jmpl_value_h
#define c_jmpl_value_h

#include <string.h>

#include "common.h"

typedef struct GC GC;
typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NULL  1 // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE  3 // 11

typedef uint64_t Value;

// Type Check

#define IS_BOOL(value)   (((value) | 1) == TRUE_VAL)
#define IS_NULL(value)   ((value) == NULL_VAL)
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_OBJ(value)    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define IS_INTEGER(value) (IS_NUMBER(value) && (int)(AS_NUMBER(value)) == AS_NUMBER(value))

// JMPL Value to C value

#define AS_BOOL(value)   ((value == TRUE_VAL))
#define AS_NUMBER(value) valueToNum(value)
#define AS_OBJ(value)    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

// C value to JMPL Value

#define BOOL_VAL(b)      ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL        ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL         ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NULL_VAL         ((Value)(uint64_t)(QNAN | TAG_NULL))
#define NUMBER_VAL(num)  numToValue(num)
#define OBJ_VAL(obj)     ((Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj)))

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

/**
 * @brief The type of a Value.
 */
typedef enum {
    VAL_BOOL,
    VAL_NULL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

/**
 * @brief The JMPL representation of a value.
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

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)

// C value to JMPL Value

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NULL_VAL          ((Value){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

// Convert values to strings

#define BOOL_TO_STRING(value) ((value) ? "true" : "false")
#define NULL_TO_STRING ("null")
#define NUMBER_TO_STRING(value, resultPtr) \
    do { \
        if((value) == (int)(value)) { \
            int size = snprintf(NULL, 0, "%d", (int)(value)) + 1; \
            *(resultPtr) = (unsigned char*)malloc(size); \
            snprintf(*(resultPtr), size, "%d", (int)(value)); \
        } else { \
            int size = snprintf(NULL, 0, "%lf", (value)) + 1; \
            *(resultPtr) = (unsigned char*)malloc(size); \
            snprintf(*(resultPtr), size, "%lf", (value)); \
        } \
    } while(false)

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(GC* gc, ValueArray* array, Value value);
void freeValueArray(GC* gc, ValueArray* array);
int findInValueArray(ValueArray* array, Value value);

bool valuesEqual(Value a, Value b);
unsigned char* valueToString(Value value);
uint32_t hashValue(Value value);
void printValue(Value value, bool simple);

#endif