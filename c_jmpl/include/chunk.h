#ifndef c_jmpl_chunk_h
#define c_jmpl_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_MOD,
    OP_DIVIDE,
    OP_EXPONENT,
    OP_NOT,
    OP_NEGATE,
    OP_OUT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_FALSE_2, // Pops condition if false
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_SET_CREATE,
    OP_SET_INSERT,
    OP_SET_INSERT_2,
    OP_SET_OMISSION,
    OP_SET_IN,
    OP_SET_INTERSECT,
    OP_SET_UNION,
    OP_SIZE,
    OP_SET_DIFFERENCE,
    OP_SUBSET,
    OP_SUBSETEQ,
    OP_CREATE_TUPLE,
    OP_TUPLE_OMISSION,
    OP_SUBSCRIPT,
    OP_CREATE_ITERATOR,
    OP_ITERATE
} OpCode;

typedef struct {
    int offset;
    int line;   
} LineStart;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;

    int lineCount;
    int lineCapacity;
    LineStart* lines;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
int getLine(Chunk* chunk, int instruction);
int findConstant(Chunk* chunk, Value value);

#endif