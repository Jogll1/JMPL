#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "native.h"

// General purpose

Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// Maths library

Value sinNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

Value cosNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

Value tanNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(tan(AS_NUMBER(args[0])));
}

Value arcsinNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(asin(AS_NUMBER(args[0])));
}

Value arccosNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(acos(AS_NUMBER(args[0])));
}

Value arctanNative(int argCount, Value* args) {
    if(!IS_NUMBER(args[0])) return NULL_VAL; 

    return NUMBER_VAL(atan(AS_NUMBER(args[0])));
}