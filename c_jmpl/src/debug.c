#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"

// --- DEBUG TOKENS ---

/**
 * @brief Return a token type's name.
 * 
 * @param type The type to translate.
 */
const unsigned char* getTokenName(TokenType type) {
    switch (type) {
        case TOKEN_LEFT_PAREN:    return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN:   return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACE:    return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE:   return "RIGHT_BRACE";
        case TOKEN_LEFT_SQUARE:   return "LEFT_SQUARE";
        case TOKEN_RIGHT_SQUARE:  return "RIGHT_SQUARE";
        case TOKEN_COMMA:         return "COMMA";
        case TOKEN_DOT:           return "DOT";
        case TOKEN_MINUS:         return "MINUS";
        case TOKEN_PLUS:          return "PLUS";
        case TOKEN_SLASH:         return "SLASH";
        case TOKEN_ASTERISK:      return "ASTERISK";
        case TOKEN_BACK_SLASH:    return "BACK_SLASH";
        case TOKEN_EQUAL:         return "EQUAL";
        case TOKEN_CARET:         return "CARET";
        case TOKEN_MOD:           return "MOD";
        case TOKEN_SEMICOLON:     return "SEMICOLON";
        case TOKEN_COLON:         return "COLON";
        case TOKEN_PIPE:          return "PIPE";
        case TOKEN_IN:            return "IN";
        case TOKEN_HASHTAG:       return "HASHTAG";
        case TOKEN_INTERSECT:     return "INTERSECT";
        case TOKEN_UNION:         return "UNION";
        case TOKEN_SUBSET:        return "SUBSET";
        case TOKEN_SUBSETEQ:      return "SUBSETEQ";
        case TOKEN_FORALL:        return "FORALL";
        case TOKEN_EXISTS:        return "EXISTS";

        case TOKEN_ASSIGN:        return "ASSIGN";
        case TOKEN_ELLIPSIS:      return "ELLIPSIS";
        case TOKEN_NOT:           return "NOT";
        case TOKEN_NOT_EQUAL:     return "NOT_EQUAL";
        case TOKEN_GREATER:       return "GREATER";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_LESS:          return "LESS";
        case TOKEN_LESS_EQUAL:    return "LESS_EQUAL";
        case TOKEN_MAPS_TO:       return "MAPS_TO";
        case TOKEN_IMPLIES:       return "IMPLIES";

        case TOKEN_IDENTIFIER:    return "IDENTIFIER";
        case TOKEN_STRING:        return "STRING";
        case TOKEN_NUMBER:        return "NUMBER";
        case TOKEN_CHAR:          return "CHAR";

        case TOKEN_AND:           return "AND";
        case TOKEN_OR:            return "OR";
        case TOKEN_XOR:           return "XOR";
        case TOKEN_TRUE:          return "TRUE";
        case TOKEN_FALSE:         return "FALSE";
        case TOKEN_LET:           return "LET";
        case TOKEN_NULL:          return "NULL";
        case TOKEN_IF:            return "IF";
        case TOKEN_THEN:          return "THEN";
        case TOKEN_ELSE:          return "ELSE";
        case TOKEN_WHILE:         return "WHILE";
        case TOKEN_DO:            return "DO";
        case TOKEN_FOR:           return "FOR";
        case TOKEN_SOME:          return "SOME";
        case TOKEN_ARB:           return "ARB";
        case TOKEN_RETURN:        return "RETURN";
        case TOKEN_FUNCTION:      return "FUNCTION";

        case TOKEN_NEWLINE:       return "NEWLINE";
        case TOKEN_INDENT:        return "INDENT";
        case TOKEN_DEDENT:        return "DEDENT";
        
        case TOKEN_ERROR:         return "ERROR";
        case TOKEN_EOF:           return "EOF";

        default:                  return "UNKNOWN";
    }
}

// --- DEBUG BYTECODE ---

// Disassemble each instruction of bytecode
void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint16_t constant = (uint16_t)(chunk->code[offset + 1] << 8);
    constant |= chunk->code[offset + 2];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant], false);
    printf("'\n");
    return offset + 3;
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int closureInstruction(const char* name, Chunk* chunk, int offset) {
    offset++;
    uint16_t constant = (uint16_t)(chunk->code[offset++] << 8);
    constant |= chunk->code[offset++];
    printf("%-16s %4d ", name, constant);
    printValue(chunk->constants.values[constant], false);
    printf("\n");

    ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
    for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                   %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
    }

    return offset;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    int line = getLine(chunk, offset);

    // If the instruction comes from the same source line as the preceeding one
    if (offset > 0 && line == getLine(chunk, offset - 1)) {
        // Print a pipe
        printf("   | ");
    } else {
        // Print the line number
        printf("%4d ", line);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:        return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NULL:            return simpleInstruction("OP_NULL", offset);
        case OP_TRUE:            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:           return simpleInstruction("OP_FALSE", offset);
        case OP_POP:             return simpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL:       return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:       return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:   return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE:     return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:     return byteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_EQUAL:           return simpleInstruction("OP_EQUAL", offset);
        case OP_NOT_EQUAL:       return simpleInstruction("OP_NOT_EQUAL", offset);
        case OP_GREATER:         return simpleInstruction("OP_GREATER", offset);
        case OP_GREATER_EQUAL:   return simpleInstruction("OP_GREATER_EQUAL", offset);
        case OP_LESS:            return simpleInstruction("OP_LESS", offset);
        case OP_LESS_EQUAL:      return simpleInstruction("OP_LESS_EQUAL", offset);
        case OP_ADD:             return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:        return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:        return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:          return simpleInstruction("OP_DIVIDE", offset);
        case OP_EXPONENT:        return simpleInstruction("OP_EXPONENT", offset);
        case OP_MOD:             return simpleInstruction("OP_MOD", offset);
        case OP_NOT:             return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:          return simpleInstruction("OP_NEGATE", offset);
        case OP_JUMP:            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:   return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP_IF_FALSE_2: return jumpInstruction("OP_JUMP_IF_FALSE_2", 1, chunk, offset);
        case OP_LOOP:            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:            return byteInstruction("OP_CALL", chunk, offset);
        case OP_CLOSURE:         return closureInstruction("OP_CLOSURE", chunk, offset);
        case OP_CLOSE_UPVALUE:   return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OP_RETURN:          return byteInstruction("OP_RETURN", chunk, offset);
        case OP_STASH:           return simpleInstruction("OP_STASH", offset);
        case OP_SET_CREATE:      return simpleInstruction("OP_SET_CREATE", offset);
        case OP_SET_INSERT:      return byteInstruction("OP_SET_INSERT", chunk, offset);
        case OP_SET_OMISSION:    return byteInstruction("OP_SET_OMISSION", chunk, offset);
        case OP_SET_IN:          return simpleInstruction("OP_SET_IN", offset);
        case OP_SET_INTERSECT:   return simpleInstruction("OP_SET_INTERSECT", offset);
        case OP_SET_UNION:       return simpleInstruction("OP_SET_UNION", offset);
        case OP_SIZE:            return simpleInstruction("OP_SIZE", offset);
        case OP_SET_DIFFERENCE:  return simpleInstruction("OP_SET_DIFFERENCE", offset);
        case OP_SUBSET:          return simpleInstruction("OP_SUBSET", offset);
        case OP_SUBSETEQ:        return simpleInstruction("OP_SUBSETEQ", offset);
        case OP_CREATE_TUPLE:    return byteInstruction("OP_CREATE_TUPLE", chunk, offset);
        case OP_TUPLE_OMISSION:  return byteInstruction("OP_TUPLE_OMISSION", chunk, offset);
        case OP_SUBSCRIPT:       return simpleInstruction("OP_SUBSCRIPT", offset);
        case OP_CREATE_ITERATOR: return simpleInstruction("OP_CREATE_ITERATOR", offset);
        case OP_ITERATE:         return simpleInstruction("OP_ITERATE", offset);
        case OP_ARB:             return simpleInstruction("OP_ARB", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}