#ifndef c_jmpl_chunk_h
#define c_jmpl_chunk_h

#include "value.h"

/**
 * @brief Opcodes for the VM.
 */
typedef enum {
  #define OPCODE(name) OP_##name,
  #include "opcodes.h"
  #undef OPCODE
  END
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
void freeChunk(GC* gc, Chunk* chunk);
void writeChunk(GC* gc, Chunk* chunk, uint8_t byte, int line);
int addConstant(GC* gc, Chunk* chunk, Value value);
int getLine(Chunk* chunk, int instruction);
int findConstant(Chunk* chunk, Value value);

#endif