#include <stdlib.h>
#include <stdio.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

/**
 * @brief Initialise empty chunk.
 * 
 * @param chunk Chunk to initialise
 */
void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->lines = NULL;

    initValueArray(&chunk->constants);
}

/**
 * @brief Deallocate all the memory of a chunk.
 * 
 * @param chunk Chunk to deallocate
 */ 
void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(LineStart, chunk->lines, chunk->lineCapacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

/**
 * @brief Append byte to the end of a chunk.
 * 
 * @param chunk Chunk to write to
 * @param byte  Byte to append to the chunk
 * @param line  Line of code where the byte is
 */
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;

        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    // If we're still on the same line
    if (chunk->lineCount > 0 && chunk->lines[chunk->lineCount - 1].line == line) {
        return;
    }

    // Append a new LineStart - compression
    if (chunk->lineCapacity < chunk->lineCount + 1) {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines = GROW_ARRAY(LineStart, chunk->lines, oldCapacity, chunk->lineCapacity);
    }

    LineStart* lineStart = &chunk->lines[chunk->lineCount++];
    lineStart->offset = chunk->count - 1;
    lineStart->line = line;
}

/**
 *  @brief Add a new constant instruction to the value array of the chunk.
 * 
 *  @param chunk The chunk to add the value to
 *  @param value Value being added to the chunk
 */ 
int addConstant(Chunk* chunk, Value value) {
    push(value);
    writeValueArray(&chunk->constants, value);
    pop();
    return chunk->constants.count - 1;
}

/**
 * @brief Write a constant to the chunk. Chooses between normal or long constant.
 * 
 * @param chunk The chunk to add the value to
 * @param value Value being added to the chunk
 * @param line  The line of the value
 * 
 * Adds the constant to the array and uses the index to see if it fits into one byte.
 * If not it uses the long opcode with the value split into multiple bytes.
 * Uses little-endian.
 */
int writeConstant(Chunk* chunk, Value value, int line) {
    int index = addConstant(chunk, value);
    
    if (index < UINT8_MAX + 1) {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, (uint8_t)index, line);
    } else {
        writeChunk(chunk, OP_CONSTANT_LONG, line);
        writeChunk(chunk, (uint8_t)(index & 0xFF), line);
        writeChunk(chunk, (uint8_t)((index) >> 8 & 0xFF), line);
        writeChunk(chunk, (uint8_t)((index) >> 16 & 0xFF), line);
    }

    return index;
}

int getLine(Chunk* chunk, int instruction) {
    int start = 0;
    int end = chunk->lineCount - 1;

    while (true) {
        int mid = (start + end) / 2;
        LineStart* line = &chunk->lines[mid];
        if (instruction < line->offset) {
            end = mid - 1;
        } else if (mid == chunk->lineCount - 1 || instruction < chunk->lines[mid + 1].offset) {
            return line->line;
        } else {
            start = mid + 1;
        }
    }
}