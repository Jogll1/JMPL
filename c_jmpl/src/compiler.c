#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <uchar.h>

#include "object.h"
#include "obj_string.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "gc.h"
#include "debug.h"
#include "utils.h"

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    Upvalue upvalues[UINT8_COUNT];
    int localCount;
    int scopeDepth;

    bool implicitReturn;
} Compiler;

typedef struct {
    Scanner scanner;
    Compiler* currentCompiler;
    GC* gc;

    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

/**
 * @brief Precedence order of operations.
 * 
 * Precedence order lowest to highest:
 * | None
 * | Assignment: :=
 * | Or:         or
 * | And:        and
 * | Equality:   ==, ¬=, in
 * | Comparison: <, >, <=, >=
 * | Term:       +, -, ∩, ∪
 * | Factor:     *, /
 * | Exponent:   ^
 * | Unary:      ¬, -
 * | Call:       ()
 * | Primary
 */ 
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // :=
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == ¬=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_EXPONENT,    // ^
    PREC_UNARY,       // ¬ -
    PREC_CALL,        // ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Parser* parser, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static Chunk* currentChunk(Parser* parser) {
    return &parser->currentCompiler->function->chunk;
}

/**
 * @brief Print the the raw characters of a string without escaping characters.
 * 
 * @param f      The file format to print
 * @param string The array of characters to print
 * @param length The length of the array of characters 
 */
static void fprintfRawString(FILE* f, const unsigned char* string, int length) {
    for (int i = 0; i < length; ++i) {
        unsigned char c = string[i];
        switch (c) {
            case '\a': fputs("\\a", f); break;
            case '\b': fputs("\\b", f); break;
            case '\e': fputs("\\e", f); break;
            case '\f': fputs("\\f", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            case '\v': fputs("\\v", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\'': fputs("\\'", f); break;
            case '\"': fputs("\\\"", f); break;
            default:
                if (isprint(c)) fputc(c, f);
                else fprintf(f, "\\x%02x", c);
        }
    }
}

static void errorAt(Parser* parser, Token* token, const unsigned char* message) {
    if(parser->panicMode) return;
    parser->panicMode = true;
    
    fprintf(stderr, "[line %d] " ANSI_RED "Error" ANSI_RESET, token->line);

    if(token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '");
        fprintfRawString(stderr, token->start, token->length);
        fprintf(stderr, "'");
    }

    fprintf(stderr, ": %s.\n", message);
    exit(INTERNAL_SOFTWARE_ERROR); // TEMPORARY HACK TO FORCE PROGRAM TO EXIT WHEN THERE'S A COMPILER ERROR
    parser->hadError = true;
}

static void error(Parser* parser, const unsigned char* message) {
    errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser* parser, const unsigned char* message) {
    errorAt(parser, &parser->current, message);
}

static void advance(Parser* parser) {
    parser->previous = parser->current;
    
    while (true) {
        parser->current = scanToken(&parser->scanner);
#ifdef DEBUG_PRINT_TOKENS
        // Debug tokens
        printf("%s", getTokenName(parser.current.type));
        printf("(");
        fprintfRawString(stderr, parser.current.start, parser.current.length);
        printf(") ");
        if (parser.current.type == TOKEN_NEWLINE || parser.current.type == TOKEN_DEDENT || parser.current.type == TOKEN_EOF) printf("\n\n");
#endif

        if(parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->current.start);
    }
}

/**
 * @brief Consumes a token of a given type. If the token is not present, returns an error.
 * 
 * @param type    The type of the token to consume
 * @param message The message to report as an error incase the token is not consumed
 * 
 * If the current token has the given token type it calls advance().
 * If not, an error is displayed with errorAtCurrent().
 */
static void consume(Parser* parser, TokenType type, const unsigned char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    errorAtCurrent(parser, message);
}

/**
 * @brief Skip zero or more newlines.
 */
static void skipNewlines(Parser* parser) {
    while (parser->current.type == TOKEN_NEWLINE) {
        advance(parser);
    }
}

static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser* parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);

    return true;
}

/**
 * @brief Consume a statement separator (newline or semicolon).
 */
static void consumeSeparator(Parser* parser) {
    // Make sure a seperator separates statements
    if (match(parser, TOKEN_SEMICOLON) || match(parser, TOKEN_NEWLINE)) return;
    
    if (check(parser, TOKEN_INDENT) || check(parser, TOKEN_DEDENT) || check(parser, TOKEN_EOF)) return;

    error(parser, "Invalid syntax");
}

static void emitByte(Parser* parser, uint8_t byte) {
    writeChunk(parser->gc, currentChunk(parser), byte, parser->previous.line);
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static void emitOpShort(Parser* parser, uint8_t byte, uint16_t u16) {
    emitByte(parser, byte);
    emitBytes(parser, (uint8_t)(u16 >> 8), (uint8_t)(u16 & 0xFF)); // Split the u16 into two bytes
}

static void emitLoop(Parser* parser, int loopStart) {
    emitByte(parser, OP_LOOP);

    int offset = currentChunk(parser)->count - loopStart + 2;
    if (offset > UINT16_MAX) error(parser, "(Internal) Loop body too large");

    emitByte(parser, (offset >> 8) & 0xFF);
    emitByte(parser, offset & 0xFF);
}

static int emitJump(Parser* parser, uint8_t instruction) {
    emitByte(parser, instruction);

    // Jump over offset
    emitByte(parser, 0xFF);
    emitByte(parser, 0xFF);

    return currentChunk(parser)->count - 2;
}

static void emitReturn(Parser* parser) {
    // Implicitly returns null if function returns nothing
    if (!parser->currentCompiler->implicitReturn || parser->currentCompiler->type != TYPE_FUNCTION) {
        emitByte(parser, OP_NULL);
    }

    emitBytes(parser, OP_RETURN, parser->currentCompiler->implicitReturn);
}

static uint16_t makeConstant(Parser* parser, Value value) {
    int constant = findConstant(currentChunk(parser), value);
    if (constant == -1) {
        constant = addConstant(parser->gc, currentChunk(parser), value);
    }

    if (constant > UINT16_MAX) {
        error(parser, "(Internal) Too many constants in one chunk");
        return 0;
    }

    return (uint16_t)constant;
}

static void emitConstant(Parser* parser, Value value) {
    emitOpShort(parser, OP_CONSTANT, makeConstant(parser, value));
}

static void patchJump(Parser* parser, int offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = currentChunk(parser)->count - offset - 2;

    if (jump > UINT16_MAX) {
        error(parser, "(Internal) Too much code to jump over");
    }

    currentChunk(parser)->code[offset] = (jump >> 8) & 0xFF;
    currentChunk(parser)->code[offset + 1] = jump & 0xFF;
}

static void initCompiler(Parser* parser, Compiler* compiler, FunctionType type) {
    compiler->enclosing = parser->currentCompiler;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->implicitReturn = false;
    compiler->function = newFunction(parser->gc);

    parser->currentCompiler = compiler;

    if (type != TYPE_SCRIPT) {
        parser->currentCompiler->function->name = copyString(parser->gc, parser->previous.start, parser->previous.length);
    }

    Local* local = &parser->currentCompiler->locals[parser->currentCompiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* endCompiler(Parser* parser) {
    emitReturn(parser);
    ObjFunction* function = parser->currentCompiler->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError) {
        disassembleChunk(currentChunk(parser), function->name != NULL ? (const char*)function->name->utf8 : "<script>");
    }
#endif

    parser->currentCompiler = parser->currentCompiler->enclosing;
    return function;
}

static void beginScope(Parser* parser) {
    parser->currentCompiler->scopeDepth++;
}

static void endScope(Parser* parser) {
    parser->currentCompiler->scopeDepth--;

    while (parser->currentCompiler->localCount > 0 && parser->currentCompiler->locals[parser->currentCompiler->localCount - 1].depth > parser->currentCompiler->scopeDepth) {
        if (parser->currentCompiler->locals[parser->currentCompiler->localCount - 1].isCaptured) {
            emitByte(parser, OP_CLOSE_UPVALUE);
        } else {
            emitByte(parser, OP_POP);
        }
        parser->currentCompiler->localCount--;
    }
}

// Function declarations 
static void expression(Parser* parser, bool ignoreNewlines);
static void statement(Parser* parser, bool blockAllowed, bool ignoreSeparator);
static void expressionStatement(Parser* parser);
static void declaration(Parser* parser);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Parser* parser, Precedence precendence, bool ignoreNewlines);

static uint16_t identifierConstant(Parser* parser, Token* name) {
    return makeConstant(parser, OBJ_VAL(copyString(parser->gc, name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Parser* parser, Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];

        if (identifiersEqual(name, &local->name)) {
            if(local->depth == -1) {
                error(parser, "Can't read local variable in its own initialiser");
            }

            return i;
        }
    }

    return -1;
}

static int addUpvalue(Parser* parser, Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error(parser, "(Internal) Too many closure variables in function");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;

    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Parser* parser, Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(parser, compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(parser, compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(parser, compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(parser, compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Parser* parser, Token name) {
    if (parser->currentCompiler->localCount == UINT8_COUNT) {
        error(parser, "(Internal) Too many local variables in current scope");
        return;
    }

    Local* local = &parser->currentCompiler->locals[parser->currentCompiler->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

/**
 * @brief Declare a local variable.
 * 
 * @return Whether the variable is already defined
 */
static void declareVariable(Parser* parser) {
    if (parser->currentCompiler->scopeDepth == 0) return;

    Token* name = &parser->previous;

    for (int i = parser->currentCompiler->localCount - 1; i >= 0; i--) {
        Local* local = &parser->currentCompiler->locals[i];

        if (local->depth != -1 && local->depth < parser->currentCompiler->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error(parser, "Variable with this identifier already defined in this scope");
        }
    }

    addLocal(parser, *name);
}

/**
 * @brief Parse a variable.
 * 
 * @param errorMessage Error to return if an identifier is not found
 * @return             The index of a variable if it is a global
 * 
 * Note: local variables live on the stack so are accessed via a stack index
 */
static uint16_t parseVariable(Parser* parser, const unsigned char* errorMessage) {
    consume(parser, TOKEN_IDENTIFIER, errorMessage);

    declareVariable(parser);
    if (parser->currentCompiler->scopeDepth > 0) return 0;

    return identifierConstant(parser, &parser->previous);
}

static void markInitialised(Parser* parser) {
    if (parser->currentCompiler->scopeDepth == 0) return;
    parser->currentCompiler->locals[parser->currentCompiler->localCount - 1].depth = parser->currentCompiler->scopeDepth;
}

static void defineVariable(Parser* parser, uint16_t global) {
    if (parser->currentCompiler->scopeDepth > 0) {
        markInitialised(parser);
        return;
    }

    emitOpShort(parser, OP_DEFINE_GLOBAL, global);
}

static Token syntheticToken(const char* name) {
  Token token;
  token.start = name;
  token.length = (int)strlen(name);
  return token;
}

/**
 * @brief Create a synthetic local variables from top of stack for internal use.
 * 
 * @param code An opcode that pushes a value to the top of the stack
 * @param name The name of the synthetic variable
 */
static uint8_t syntheticLocal(Parser* parser, OpCode code, const char* name) {
    emitByte(parser, code);
    
    uint8_t varSlot = parser->currentCompiler->localCount;
    addLocal(parser, syntheticToken(name));
    markInitialised(parser);
    emitBytes(parser, OP_SET_LOCAL, varSlot);

    return varSlot;
}

/**
 * @brief Parse a generator in the form 'x in Obj'.
 * 
 * @return The slot of the generator
 * 
 * Pushes: a local variable, a null value initialiser, and the target object
 */
static uint8_t parseGenerator(Parser* parser) {
    // Parse the local variable that will be the generator
    uint8_t localVarSlot = parser->currentCompiler->localCount;
    consume(parser, TOKEN_IDENTIFIER, "Expected identifier");
    
    // === Check if name is already defined ===
    Token* name = &parser->previous;

    for (int i = parser->currentCompiler->localCount - 1; i >= 0; i--) {
        Local* local = &parser->currentCompiler->locals[i];

        if (local->depth != -1 && local->depth < parser->currentCompiler->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            return 0;
        }
    }
    // ========================================

    addLocal(parser, *name);

    emitByte(parser, OP_NULL); // Set it to null initially
    defineVariable(parser, localVarSlot);

    consume(parser, TOKEN_IN, "Expected 'in' or '∈' after identifier");

    // Push the object to generate from (to create an iterator)
    expression(parser, false);

    return localVarSlot;
}

/**
 * @brief Creates and closes a new compiler around a c function call.
 * 
 * @param type The type of function to create
 * @param f    The c function to call to compile the inner part of the function
 */
static void functionWrapper(Parser* parser, FunctionType type, void (*f)()) {
    Compiler compiler;
    initCompiler(parser, &compiler, type);
    beginScope(parser);

    (*f)(parser);

    ObjFunction* function = endCompiler(parser);
    emitOpShort(parser, OP_CLOSURE, makeConstant(parser, OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(parser, compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(parser, compiler.upvalues[i].index);
    }
}

static uint8_t argumentList(Parser* parser) {
    uint8_t argCount = 0;
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            skipNewlines(parser);
            expression(parser, true);

            if(argCount == 255) {
                error(parser, "(Internal) Can't have more than 255 arguments");
            }
            argCount++;
        } while (match(parser, TOKEN_COMMA));
    }

    skipNewlines(parser);

    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
    return argCount;
}

static void binary(Parser* parser, bool canAssign) {
    TokenType operatorType = parser->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(parser, (Precedence)(rule->precedence + 1), false);

    switch (operatorType) {
        case TOKEN_NOT_EQUAL:     emitByte(parser, OP_NOT_EQUAL);      break;
        case TOKEN_EQUAL_EQUAL:   emitByte(parser, OP_EQUAL);          break;
        case TOKEN_GREATER:       emitByte(parser, OP_GREATER);        break;
        case TOKEN_GREATER_EQUAL: emitByte(parser, OP_GREATER_EQUAL);  break;
        case TOKEN_LESS:          emitByte(parser, OP_LESS);           break;
        case TOKEN_LESS_EQUAL:    emitByte(parser, OP_LESS_EQUAL);     break;
        case TOKEN_PLUS:          emitByte(parser, OP_ADD);            break;
        case TOKEN_MINUS:         emitByte(parser, OP_SUBTRACT);       break;
        case TOKEN_ASTERISK:      emitByte(parser, OP_MULTIPLY);       break;
        case TOKEN_SLASH:         emitByte(parser, OP_DIVIDE);         break;
        case TOKEN_CARET:         emitByte(parser, OP_EXPONENT);       break;
        case TOKEN_MOD:           emitByte(parser, OP_MOD);            break;
        case TOKEN_IN:            emitByte(parser, OP_SET_IN);         break;
        case TOKEN_INTERSECT:     emitByte(parser, OP_SET_INTERSECT);  break;
        case TOKEN_UNION:         emitByte(parser, OP_SET_UNION);      break;
        case TOKEN_BACK_SLASH:    emitByte(parser, OP_SET_DIFFERENCE); break;
        case TOKEN_SUBSET:        emitByte(parser, OP_SUBSET);         break;
        case TOKEN_SUBSETEQ:      emitByte(parser, OP_SUBSETEQ);       break;
        default: return;
    }
}

static void call(Parser* parser, bool canAssign) {
    uint8_t argCount = argumentList(parser);
    emitBytes(parser, OP_CALL, argCount);
}

static void subscript(Parser* parser, bool canAssign) {
    int isSlice = 0;

    if (match(parser, TOKEN_ELLIPSIS)) {
        // [... x]
        isSlice = 1;
        emitByte(parser, OP_NULL);
        expression(parser, true);
    } else {
        expression(parser, true);

        if (match(parser, TOKEN_ELLIPSIS)) {
            isSlice = 1;
            if (check(parser, TOKEN_RIGHT_SQUARE)) {
                // [x ...]
                emitByte(parser, OP_NULL);
            } else {
                // [x ... y]
                expression(parser, true);
            }
        }
    }

    consume(parser, TOKEN_RIGHT_SQUARE, "Expected ']' after expression");
    emitBytes(parser, OP_SUBSCRIPT, isSlice);
}

static void literal(Parser* parser, bool canAssign) {
    switch (parser->previous.type) {
        case TOKEN_FALSE: emitByte(parser, OP_FALSE); break;
        case TOKEN_NULL:  emitByte(parser, OP_NULL);  break;
        case TOKEN_TRUE:  emitByte(parser, OP_TRUE);  break;
        default: return;
    }
}

/**
 * @brief Parses a tuple.
 * 
 * Can parse tuples in format: (x, y, z, etc)
 * or:                         (x,)
 * or:                         (f ... l)
 * or:                         (f, n, ... l)
 */
static void tuple(Parser* parser) {
    if (check(parser, TOKEN_ELLIPSIS)) {
        // Omission operation without 'next'
        advance(parser);
        expression(parser, true);
        emitBytes(parser, OP_TUPLE_OMISSION, 0);
    } else {
        if (match(parser, TOKEN_COMMA)) {
            if (check(parser, TOKEN_RIGHT_PAREN)) {
                // 1-tuple
                emitBytes(parser, OP_CREATE_TUPLE, 1);
                return;
            }

            expression(parser, true);

            if (check(parser, TOKEN_ELLIPSIS)) {
                // Omission operation with 'next'
                advance(parser);
                expression(parser, true);
                emitBytes(parser, OP_TUPLE_OMISSION, 1);
            } else {
                // Normal tuple construction
                int count = 2;
                while (match(parser, TOKEN_COMMA)) {
                    expression(parser, true);
                    if (count < UINT8_MAX) {
                        count++;
                    } else {
                        error(parser, "(Internal) Can't have more than 255 elements in a tuple literal");
                    }
                }

                emitBytes(parser, OP_CREATE_TUPLE, count);
            }
        }
    }
}

static void grouping(Parser* parser, bool canAssign) {
    // If its an empty parentheses, make an empty tuple
    if (check(parser, TOKEN_RIGHT_PAREN)) {
        advance(parser);
        emitBytes(parser, OP_CREATE_TUPLE, 0);
        return;
    }

    expression(parser, true);

    // Could be a tuple
    tuple(parser);  

    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

// ======================================================================
// =================        Set builder notation        =================
// ======================================================================

static void growArray(uint8_t** array, size_t currentSize, uint8_t newVal) {
    uint8_t* newArray = realloc(*array, (currentSize + 1) * sizeof(uint8_t));
    if (newArray == NULL) {
        exit(INTERNAL_SOFTWARE_ERROR);
    }

    newArray[currentSize] = newVal;
    *array = newArray;
}

static bool parseSetBuilderGenerator(Parser* parser, int oldCount, uint8_t** generatorSlots, uint8_t** iteratorSlots, uint8_t** loopStarts, uint8_t** exitJumps) {
    Parser temp = *parser;
    // Check if its a generator 'x ∈'
    bool isGenerator = match(parser, TOKEN_IDENTIFIER) && match(parser, TOKEN_IN);
    *parser = temp;

    if (isGenerator) {
        uint8_t generatorSlot = parseGenerator(parser);
        if (generatorSlot == 0) {
            *parser = temp;
            return false; // Return false if already defined
        }

        uint8_t iteratorSlot = syntheticLocal(parser, OP_CREATE_ITERATOR, "@iter");

        growArray(generatorSlots, oldCount, generatorSlot);
        growArray(iteratorSlots, oldCount, iteratorSlot);

        // Generate loop
        int loopStart = currentChunk(parser)->count;
        emitBytes(parser, OP_GET_LOCAL, iteratorSlot);
        emitByte(parser, OP_ITERATE);

        int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
        emitByte(parser, OP_POP); // Pop check

        emitBytes(parser, OP_SET_LOCAL, generatorSlot);
        emitByte(parser, OP_POP);

        growArray(loopStarts, oldCount, loopStart);
        growArray(exitJumps, oldCount, exitJump);

        return true;
    }

    return false;
}

/**
 * @brief Parse a set builder.
 */
static void setBuilder(Parser* parser) {
    // Set anonymous function name and to implicit return
    parser->currentCompiler->function->name = copyString(parser->gc, "@setb", 5);
    parser->currentCompiler->implicitReturn = true;

    // Store an opened set as a local
    uint8_t setSlot = syntheticLocal(parser, OP_SET_CREATE, "@set");

    uint8_t* generatorSlots = NULL;
    uint8_t* iteratorSlots = NULL;
    uint8_t* loopStarts = NULL;
    uint8_t* exitJumps = NULL;
    int generatorCount = 0;

    uint8_t* skipJumps = NULL;
    int skipCount = 0;

    // Check if expression is a generator, otherwise skip to pipe
    Parser initialParser = *parser;
    bool hasLHSGenerator = false;
    if (parseSetBuilderGenerator(parser, generatorCount, &generatorSlots, &iteratorSlots, &loopStarts, &exitJumps)) {
        generatorCount++;
        hasLHSGenerator = true;
    } else {
        while (!check(parser, TOKEN_PIPE)) advance(parser);
    }
    consume(parser, TOKEN_PIPE, "Expected '|' after expression or generator");

    // Parse qualifiers (generators and filters)
    bool hasRHS = false;
    do {
        if (check(parser, TOKEN_RIGHT_BRACE)) break;
        hasRHS = true;

        // Check if it is a generator
        if (parseSetBuilderGenerator(parser, generatorCount, &generatorSlots, & iteratorSlots, &loopStarts, &exitJumps)) {
            generatorCount++;
            continue;
        }

        // Not a generator, so a predicate
        expression(parser, false);
        int skipJump = emitJump(parser, OP_JUMP_IF_FALSE_2);
        emitByte(parser, OP_POP);

        growArray(&skipJumps, skipCount, skipJump);
        skipCount++;
    } while (match(parser, TOKEN_COMMA));

    if (!hasRHS) errorAtCurrent(parser, "Set-builder must have at one qualifier");
    if (generatorCount == 0) errorAtCurrent(parser, "Set-builder must have at least one generator");
    Parser endParser = *parser;
    *parser = initialParser;

    // Load set and insert expression
    emitBytes(parser, OP_GET_LOCAL, setSlot);
    if (hasLHSGenerator) {
        emitBytes(parser, OP_GET_LOCAL, generatorSlots[0]);
    } else {
        expression(parser, false);
    }
    emitBytes(parser, OP_SET_INSERT, 1); // This seems to update without OP_SET_LOCAL - pointer fault?
    emitByte(parser, OP_POP);

    // Patch jumps and emit loops
    for (int i = skipCount - 1; i >= 0; i--) {
        patchJump(parser, skipJumps[i]);
    }

    for (int i = generatorCount - 1; i >= 0; i--) {
        emitLoop(parser, loopStarts[i]);
        patchJump(parser, exitJumps[i]);
        emitByte(parser, OP_POP);
    }
    
    *parser = endParser;
    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after set-builder");
    emitBytes(parser, OP_GET_LOCAL, setSlot); // Push the completed set
    emitByte(parser, OP_STASH);

    free(generatorSlots);
    free(iteratorSlots);
    free(loopStarts);
    free(exitJumps);
    free(skipJumps);
}

/**
 * @brief Helper function to call setBuilder if a '|' is found.
 *
 * Assumes a left brace has been opened.
 */
static bool isSetBuilder(Parser* parser) {
    Parser initialParser = *parser;
    // Currently in an open brace, braceDepth should end at 1 or less in case of nested sets
    int braceDepth = 1; 

    // Find the pipe (to show we are in a set builder)
    while (!check(parser, TOKEN_PIPE) && !check(parser, TOKEN_EOF)) {
        if (check(parser, TOKEN_LEFT_BRACE)) braceDepth++;
        if (check(parser, TOKEN_RIGHT_BRACE)) braceDepth--;
        if (braceDepth == 0) break;

        advance(parser);
    }

    bool isBuilder = check(parser, TOKEN_PIPE);
    *parser = initialParser;
    if (!isBuilder) return false;

    // Call set builder and implicitly return its value
    functionWrapper(parser, TYPE_FUNCTION, setBuilder);
    emitBytes(parser, OP_CALL, 0);
    return true;
}

// ======================================================================
// ======================================================================
// ======================================================================

/**
 * @brief Parses a set.
 * 
 * Can parse sets in format: {x, y, z},
 * or:                       {f ... l},
 * or:                       {f, n, ..., l}
 */
static void set(Parser* parser, bool canAssign) {
    if (!check(parser, TOKEN_RIGHT_BRACE)) {
        // Check if its a set builder
        if (isSetBuilder(parser)) return;
        
        emitByte(parser, OP_SET_CREATE);
        expression(parser, true);

        if (check(parser, TOKEN_ELLIPSIS)) {
            // Omission operation without 'next'
            advance(parser);
            expression(parser, true);
            emitBytes(parser, OP_SET_OMISSION, 0);
        } else {
            if (match(parser, TOKEN_COMMA)) {
                expression(parser, true);

                if (check(parser, TOKEN_ELLIPSIS)) {
                    // Omission operation with 'next'
                    advance(parser);
                    expression(parser, true);
                    emitBytes(parser, OP_SET_OMISSION, 1);
                } else {
                    // Normal set construction
                    emitBytes(parser, OP_SET_INSERT, 2);

                    int count = 0;
                    while (match(parser, TOKEN_COMMA)) {
                        expression(parser, true);
                        count++;
                    }
                    if (count > 0) emitBytes(parser, OP_SET_INSERT, count);
                }
            } else {
                // Singleton set
                emitBytes(parser, OP_SET_INSERT, 1);
            }
        }
    } else {
        // Empty set
        emitByte(parser, OP_SET_CREATE);
    }
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after set literal");
}
// ----------------------

static void number(Parser* parser, bool canAssign) {
    // Convert string to double
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(value));
}

static void and_(Parser* parser, bool canAssign) {
    // If left operand is false (currently on stack), emit call to jump to the end
    int endJump = emitJump(parser, OP_JUMP_IF_FALSE);
    
    // Pop left operand if true and evaluate right operand
    emitByte(parser, OP_POP);
    parsePrecedence(parser, PREC_AND, false);
    
    patchJump(parser, endJump);
}

static void or_(Parser* parser, bool canAssign) {
    int elseJump = emitJump(parser, OP_JUMP_IF_FALSE);
    int endJump = emitJump(parser, OP_JUMP);

    patchJump(parser, elseJump);
    emitByte(parser, OP_POP);

    parsePrecedence(parser, PREC_OR, false);
    patchJump(parser, endJump);
}

/**
 * @brief Convert escape sequences into escape characters in a string of UTF-8 bytes.
 * 
 * @param output An empty output string
 * @param chars  The first character of the string
 * @param length The length of the string
 * @returns      The size of the new string
 */
static size_t decodeEscapeString(Parser* parser, unsigned char* output, const unsigned char* chars, size_t length) {
    size_t outputLength = 0;

    size_t i = 0;
    while (i < length) {
        if (chars[i] != '\\') {
            output[outputLength++] = chars[i++];
            continue;
        }

        if (i + 1 >= length) {
            errorAtCurrent(parser, "Incomplete escape sequence");
        }

        unsigned char escChar = chars[++i]; // Char after '\'
        EscapeType type = getEscapeType(escChar);

        if (type == ESC_SIMPLE) {
            output[outputLength++] = decodeSimpleEscape(escChar);
            i++;
        } else if (type == ESC_HEX || type == ESC_UNICODE || type == ESC_UNICODE_LG) {
            size_t hexCount = type; // Type is encoded as the number of hex digits
            if (i + hexCount >= length) {
                errorAtCurrent(parser, "Incomplete hex/unicode escape sequence");
            }

            uint32_t codePoint = 0;
            for (size_t j = 0; j < hexCount; j++) {
                unsigned char c = chars[++i];
                if (!isHex(c)) {
                    errorAtCurrent(parser, "Invalid hex digit in escape");
                }
                codePoint = (codePoint << 4) | hexToValue(c);
            }
            i++;

            unsigned char utf8Buf[4];
            size_t bytes = unicodeToUtf8(codePoint, utf8Buf);
            for (size_t j = 0; j < bytes; j++) {
                output[outputLength++] = utf8Buf[j];
            }
        } else {
            errorAtCurrent(parser, "Unknown escape sequence");
        }
    }
    
    return outputLength;
}

static void character(Parser* parser, bool canAssign) {
    // Copy the char bytes from the source, +1 and -2 to trim quotation mark
    size_t length = parser->previous.length - 2;
    unsigned char decoded[length];
    size_t newLength = decodeEscapeString(parser, decoded, parser->previous.start + 1, length);

    uint32_t value = utf8ToUnicode(decoded, newLength);

    if (value > UNICODE_MAX) {
        errorAtCurrent(parser, "Unsupported character");
    }

    emitConstant(parser, CHAR_VAL(value));
}

static void string(Parser* parser, bool canAssign) {
    // Copy the string from the source, +1 and -2 to trim quotation marks
    size_t length = parser->previous.length - 2;
    unsigned char decoded[length];
    size_t newLength = decodeEscapeString(parser, decoded, parser->previous.start + 1, length);

    Value s = OBJ_VAL(copyString(parser->gc, decoded, newLength));

    pushTemp(parser->gc, s);
    emitConstant(parser, s);
    popTemp(parser->gc);
}

static void namedVariable(Parser* parser, Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(parser, parser->currentCompiler, &name);

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(parser, parser->currentCompiler, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(parser, &name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(parser, TOKEN_ASSIGN)) {
        expression(parser, false);

        if (setOp == OP_SET_GLOBAL) {
            emitOpShort(parser, setOp, (uint16_t)arg);
        } else {
            emitBytes(parser, setOp, (uint8_t)arg);
        }
    } else {
        if (getOp == OP_GET_GLOBAL) {
            emitOpShort(parser, getOp, (uint16_t)arg);
        } else {
            emitBytes(parser, getOp, (uint8_t)arg);
        }
    }
}

static void variable(Parser* parser, bool canAssign) {
    namedVariable(parser, parser->previous, canAssign);
}

static void unary(Parser* parser, bool canAssign) {
    TokenType operatorType = parser->previous.type;

    // Compile the operand
    parsePrecedence(parser, PREC_UNARY, false);

    // Emit the operator instruction
    switch (operatorType) {
        case TOKEN_NOT:     emitByte(parser, OP_NOT);    break;
        case TOKEN_MINUS:   emitByte(parser, OP_NEGATE); break;
        case TOKEN_PLUS:    break;
        case TOKEN_HASHTAG: emitByte(parser, OP_SIZE);   break;
        case TOKEN_ARB:     emitByte(parser, OP_ARB);   break;
        default: return;
    } 
}

static void quantifier(Parser* parser) {
    // Set anonymous function name and to implicit return
    parser->currentCompiler->function->name = copyString(parser->gc, "@quan", 5);
    parser->currentCompiler->implicitReturn = true;

    TokenType operatorType = parser->previous.type;

    // === FOR LOAN CODE ===
    uint8_t loopVarSlot = parseGenerator(parser);
    if (loopVarSlot == 0) error(parser, "Variable with this identifier already defined in this scope");
    uint8_t iteratorSlot = syntheticLocal(parser, OP_CREATE_ITERATOR, "@iter");

    int loopStart = currentChunk(parser)->count;

    // Load and iterate the iterator
    emitBytes(parser, OP_GET_LOCAL, iteratorSlot);
    emitByte(parser, OP_ITERATE); // Push next value + bool for if there is a current value

    // If no current value -> jump to after-loop
    int loopEnd = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP); // Pop check

    // If loop is fine, set iterative variable
    emitBytes(parser, OP_SET_LOCAL, loopVarSlot);
    emitByte(parser, OP_POP);
    // =====================

    // Predicate
    consume(parser, TOKEN_PIPE, "Expected pipe after generator of quantifier");
    expression(parser, false);

    // Invert expression for exists and some
    if (operatorType == TOKEN_EXISTS || operatorType == TOKEN_SOME) {
        emitByte(parser, OP_NOT);
    }

    int loopEarlyExit = emitJump(parser, OP_JUMP_IF_FALSE_2);
    emitByte(parser, OP_POP);

    emitLoop(parser, loopStart);

    // Early exit
    patchJump(parser, loopEarlyExit);

    if (operatorType == TOKEN_SOME) {
        emitBytes(parser, OP_GET_LOCAL, loopVarSlot);
    } else {
        emitByte(parser, operatorType == TOKEN_FORALL ? OP_FALSE : OP_TRUE);
    }

    emitByte(parser, OP_STASH);
    emitBytes(parser, OP_RETURN, parser->currentCompiler->implicitReturn); // Return manually

    // Loop end
    patchJump(parser, loopEnd);
    emitByte(parser, OP_POP);

    if (operatorType == TOKEN_SOME) {
        emitByte(parser, OP_NULL);
    } else {
        emitByte(parser, operatorType == TOKEN_FORALL ? OP_TRUE : OP_FALSE); 
    }

    emitByte(parser, OP_STASH);
}

/**
 * @brief Wrapper for quantifier to call it as an anonymous function.
 */
static void quantWrap(Parser* parser, bool canAssign) {
    functionWrapper(parser, TYPE_FUNCTION, quantifier);
    emitBytes(parser, OP_CALL, 0);
}

static void anonymousFunction(Parser* parser) {
    // Set anonymous function name and to implicit return
    parser->currentCompiler->function->name = copyString(parser->gc, "@anon", 5);
    parser->currentCompiler->implicitReturn = true;

    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' after anonymous function declaration");

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            parser->currentCompiler->function->arity++;
            if(parser->currentCompiler->function->arity > 255) {
                errorAtCurrent(parser, "Can't have more than 255 parameters");
            }

            uint16_t constant = parseVariable(parser, "Expected parameter name");
            defineVariable(parser, constant);
        } while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after anonymous function parameters");
    consume(parser, TOKEN_MAPS_TO, "Expected '->' or '→' after anonymous function signature");

    // Compile the body as an expression
    expressionStatement(parser);
}

/**
 * @brief Wrapper for creating an anonymous function.
 */ 
static void anonWrap(Parser* parser, bool canAssign) {
    functionWrapper(parser, TYPE_FUNCTION, anonymousFunction);
}

ParseRule rules[] = {
    // Rules for parsing expressions
    // Token name            prefix      infix      precedence
    [TOKEN_LEFT_PAREN]    = {grouping,   call,      PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,       NULL,      PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {set,        NULL,      PREC_PRIMARY}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,       NULL,      PREC_NONE},
    [TOKEN_LEFT_SQUARE]   = {NULL,       subscript, PREC_CALL},
    [TOKEN_RIGHT_SQUARE]  = {NULL,       NULL,      PREC_NONE},
    [TOKEN_COMMA]         = {NULL,       NULL,      PREC_NONE},
    [TOKEN_DOT]           = {NULL,       NULL,      PREC_NONE},
    [TOKEN_ELLIPSIS]      = {NULL,       NULL,      PREC_NONE},
    [TOKEN_MINUS]         = {unary,      binary,    PREC_TERM},
    [TOKEN_PLUS]          = {unary,      binary,    PREC_TERM},
    [TOKEN_SLASH]         = {NULL,       binary,    PREC_FACTOR},
    [TOKEN_ASTERISK]      = {NULL,       binary,    PREC_FACTOR},
    [TOKEN_BACK_SLASH]    = {NULL,       binary,    PREC_TERM},
    [TOKEN_CARET]         = {NULL,       binary,    PREC_EXPONENT},
    [TOKEN_MOD]           = {NULL,       binary,    PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,       NULL,      PREC_NONE},
    [TOKEN_COLON]         = {NULL,       NULL,      PREC_NONE},
    [TOKEN_PIPE]          = {NULL,       NULL,      PREC_NONE},
    [TOKEN_IN]            = {NULL,       binary,    PREC_EQUALITY},
    [TOKEN_HASHTAG]       = {unary,      NULL,      PREC_UNARY},
    [TOKEN_INTERSECT]     = {NULL,       binary,    PREC_TERM},
    [TOKEN_UNION]         = {NULL,       binary,    PREC_TERM},
    [TOKEN_SUBSET]        = {NULL,       binary,    PREC_TERM},
    [TOKEN_SUBSETEQ]      = {NULL,       binary,    PREC_TERM},
    [TOKEN_FORALL]        = {quantWrap,  NULL,      PREC_EQUALITY},
    [TOKEN_EXISTS]        = {quantWrap,  NULL,      PREC_EQUALITY},
    [TOKEN_SOME]          = {quantWrap,  NULL,      PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,       NULL,      PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,       binary,    PREC_EQUALITY},
    [TOKEN_ASSIGN]        = {NULL,       NULL,      PREC_NONE},
    [TOKEN_NOT]           = {unary,      NULL,      PREC_UNARY},
    [TOKEN_NOT_EQUAL]     = {NULL,       binary,    PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,       binary,    PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,       binary,    PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,       binary,    PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,       binary,    PREC_COMPARISON},
    [TOKEN_MAPS_TO]       = {NULL,       NULL,      PREC_NONE},
    [TOKEN_IMPLIES]       = {NULL,       NULL,      PREC_NONE},
    [TOKEN_IDENTIFIER]    = {variable,   NULL,      PREC_TERM},
    [TOKEN_STRING]        = {string,     NULL,      PREC_NONE},
    [TOKEN_NUMBER]        = {number,     NULL,      PREC_NONE},
    [TOKEN_CHAR]          = {character,  NULL,      PREC_NONE},
    [TOKEN_AND]           = {NULL,       and_,      PREC_AND},
    [TOKEN_OR]            = {NULL,       or_,       PREC_OR},
    [TOKEN_XOR]           = {NULL,       NULL,      PREC_NONE},
    [TOKEN_TRUE]          = {literal,    NULL,      PREC_NONE},
    [TOKEN_FALSE]         = {literal,    NULL,      PREC_NONE},
    [TOKEN_LET]           = {NULL,       NULL,      PREC_NONE},
    [TOKEN_NULL]          = {literal,    NULL,      PREC_NONE},
    [TOKEN_IF]            = {NULL,       NULL,      PREC_NONE},
    [TOKEN_THEN]          = {NULL,       NULL,      PREC_NONE},
    [TOKEN_ELSE]          = {NULL,       NULL,      PREC_NONE},
    [TOKEN_WHILE]         = {NULL,       NULL,      PREC_NONE},
    [TOKEN_DO]            = {NULL,       NULL,      PREC_NONE},
    [TOKEN_FOR]           = {NULL,       NULL,      PREC_NONE},
    [TOKEN_ARB]           = {unary,      NULL,      PREC_UNARY},
    [TOKEN_RETURN]        = {NULL,       NULL,      PREC_NONE},
    [TOKEN_FUNCTION]      = {anonWrap,   NULL,      PREC_ASSIGNMENT},
    [TOKEN_WITH]          = {NULL,       NULL,      PREC_NONE},
    [TOKEN_NEWLINE]       = {NULL,       NULL,      PREC_NONE},
    [TOKEN_INDENT]        = {NULL,       NULL,      PREC_NONE},
    [TOKEN_DEDENT]        = {NULL,       NULL,      PREC_NONE},
    [TOKEN_ERROR]         = {NULL,       NULL,      PREC_NONE},
    [TOKEN_EOF]           = {NULL,       NULL,      PREC_NONE},
};

static void parsePrecedence(Parser* parser, Precedence precendence, bool ignoreNewlines) {
    advance(parser);

    // Ignore newline tokens if the flag is set
    if (ignoreNewlines) skipNewlines(parser);

    // Get which function to call for the prefix as the first token of an expression is always a prefix (inc. numbers)
    ParseFn prefixRule = getRule(parser->previous.type)->prefix;

    if (prefixRule == NULL) {
        error(parser, "Expected expression");
        return;
    }
    
    // Call the prefix function
    bool canAssign = precendence <= PREC_ASSIGNMENT;
    prefixRule(parser, canAssign);
    
    while (precendence <= getRule(parser->current.type)->precedence) {
        advance(parser);

        // Ignore newline tokens if the flag is set
        if (ignoreNewlines) skipNewlines(parser);
        ParseFn infixRule = getRule(parser->previous.type)->infix;

        if (infixRule == NULL) {
            error(parser, "Invalid syntax");
            return;
        }

        // Call the function for the infix operation
        infixRule(parser, canAssign);
    }

    if (canAssign && match(parser, TOKEN_ASSIGN)) {
        error(parser, "Invalid assignment target");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

/**
 * @brief Parse an expression.
 * 
 * @param ignoreNewlines Whether to ignore newline tokens
 * 
 * This requires an ignore newlines flag as the parser should ignore newlines when parsing
 * expressions such as groupings.
 */
static void expression(Parser* parser, bool ignoreNewlines) {
    parsePrecedence(parser, PREC_ASSIGNMENT, ignoreNewlines);
}

static void block(Parser* parser) {
    skipNewlines(parser);

    while (!check(parser, TOKEN_DEDENT) && !check(parser, TOKEN_EOF)) {
        declaration(parser);
    }

    consume(parser, TOKEN_DEDENT, "Expected 'DEDENT' after block");
}

static void function(Parser* parser) {
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' after function name");

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            parser->currentCompiler->function->arity++;
            if(parser->currentCompiler->function->arity > 255) {
                errorAtCurrent(parser, "Can't have more than 255 parameters");
            }

            uint16_t constant = parseVariable(parser, "Expected parameter name");
            defineVariable(parser, constant);
        } while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after function parameters");
    consume(parser, TOKEN_EQUAL, "Expected '=' after function signature");

    // Compile the body
    skipNewlines(parser);
    statement(parser, true, false);
}

static void functionDeclaration(Parser* parser) {
    uint16_t global = parseVariable(parser, "Expected function name");
    markInitialised(parser);
    functionWrapper(parser, TYPE_FUNCTION, function);
    defineVariable(parser, global);
}

static void letDeclaration(Parser* parser) {
    uint16_t global = parseVariable(parser, "Expected variable name");

    if (match(parser, TOKEN_EQUAL)) {
        // Declare a variable with an expression as its initial value
        expression(parser, false);
    } else {
        // Declare it with a null value
        emitByte(parser, OP_NULL);
    }

    defineVariable(parser, global);

    consumeSeparator(parser);
}

static void withDeclaration(Parser* parser) {
    // Consume a path
    consume(parser, TOKEN_STRING, "Expected a string after with declaration");
    uint16_t libConstant = makeConstant(parser, OBJ_VAL(copyString(parser->gc, parser->previous.start + 1, parser->previous.length - 2)));

    emitOpShort(parser, OP_IMPORT_LIB, libConstant);
    emitByte(parser, OP_POP); // Pop module result
}

static void expressionStatement(Parser* parser) {
    expression(parser, false);
    emitByte(parser, parser->currentCompiler->implicitReturn ? OP_STASH : OP_POP);
}

static void ifStatement(Parser* parser) {
    expression(parser, false);
    skipNewlines(parser);
    consume(parser, TOKEN_THEN, "Expected 'then' after condition");

    int thenJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);
    statement(parser, true, true);

    int elseJump = emitJump(parser, OP_JUMP);

    patchJump(parser, thenJump);
    emitByte(parser, OP_POP);

    skipNewlines(parser); // Skip newlines to search for else
    if (match(parser, TOKEN_ELSE)) statement(parser, true, false);
    skipNewlines(parser);
    
    patchJump(parser, elseJump);
}

static void returnStatement(Parser* parser) {
    if (parser->currentCompiler->type == TYPE_SCRIPT) {
        error(parser, "Can't return from top-level code");
    }

    if (match(parser, TOKEN_SEMICOLON) || match(parser, TOKEN_NEWLINE)) { // Interesting
        emitReturn(parser);
    } else {
        expression(parser, false);
        emitBytes(parser, OP_RETURN, 0);
    }
}

static void whileStatement(Parser* parser) {
    int loopStart = currentChunk(parser)->count;
    expression(parser, false);
    consume(parser, TOKEN_DO, "Expected 'do' after condition");

    int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP); // Pop condition to check
    skipNewlines(parser);
    statement(parser, true, false);
    emitLoop(parser, loopStart);

    patchJump(parser, exitJump);
    emitByte(parser, OP_POP);
}

static void forStatement(Parser* parser) {
    beginScope(parser);

    uint8_t loopVarSlot = parseGenerator(parser);
    if (loopVarSlot == 0) error(parser, "Variable with this identifier already defined in this scope");
    uint8_t iteratorSlot = syntheticLocal(parser, OP_CREATE_ITERATOR, "@iter");

    int loopStart = currentChunk(parser)->count;

    // Load and iterate the iterator
    emitBytes(parser, OP_GET_LOCAL, iteratorSlot);
    emitByte(parser, OP_ITERATE); // Push next value then the bool for if there is a current value

    // If no current value -> jump to after-loop
    int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP); // Pop check
    
    // If loop is fine, set iterative variable
    emitBytes(parser, OP_SET_LOCAL, loopVarSlot);
    emitByte(parser, OP_POP);

    // Compile optional predicate
    if (match(parser, TOKEN_PIPE)) {
        expression(parser, false);
        consume(parser, TOKEN_DO, "Expected expression");

        // Skip statement if predicate is false
        int skipJump = emitJump(parser, OP_JUMP_IF_FALSE_2);
        emitByte(parser, OP_POP); // Pop predicate

        statement(parser, true, false);

        // Loop and patch
        patchJump(parser, skipJump);

    } else {
        consume(parser, TOKEN_DO, "Expected expression");
        statement(parser, true, false);
    }

    emitLoop(parser, loopStart);

    patchJump(parser, exitJump);
    emitByte(parser, OP_POP);
    endScope(parser);
}

static void synchronise(Parser* parser) {
    parser->panicMode = false;

    while(parser->current.type != TOKEN_EOF) {
        if(parser->previous.type == TOKEN_SEMICOLON) return;
        switch(parser->current.type) {
            case TOKEN_FUNCTION:
            case TOKEN_LET:
            case TOKEN_WITH:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_RETURN:
                return;
            default:; // Do nothing
        }
    }

    advance(parser);
}

static void declaration(Parser* parser) {
    if (match(parser, TOKEN_FUNCTION)) {
        functionDeclaration(parser);
    } else if (match(parser, TOKEN_LET)) {
        letDeclaration(parser);
    } else if (match(parser, TOKEN_WITH)) {
        withDeclaration(parser);
    } else {
        statement(parser, false, false);
    }

    if (parser->panicMode) synchronise(parser);

    skipNewlines(parser);
}

static void statement(Parser* parser, bool blockAllowed, bool ignoreSeparator) {
    if (parser->currentCompiler->type == TYPE_SCRIPT) parser->currentCompiler->implicitReturn = false;
    
    if (match(parser, TOKEN_IF)) {
        ifStatement(parser);
    } else if (match(parser, TOKEN_RETURN)) {
        returnStatement(parser);
        if(!ignoreSeparator) consumeSeparator(parser);
    } else if (match(parser, TOKEN_WHILE)) {
        whileStatement(parser);
    } else if (match(parser, TOKEN_FOR)) {
        forStatement(parser);
    } else if(match(parser, TOKEN_INDENT)) {
        if (!blockAllowed) error(parser, "Unexpected indent");

        beginScope(parser);
        block(parser);
        endScope(parser);
    } else {
        // Set the implicit return flag if in a function
        if(parser->currentCompiler->type == TYPE_FUNCTION) {
            parser->currentCompiler->implicitReturn = true;
        } 
        expressionStatement(parser);
        if (!ignoreSeparator) consumeSeparator(parser);
    }
}

ObjFunction* compile(GC* gc, const unsigned char* source) {
    Parser parser;
    parser.currentCompiler = NULL;
    parser.gc = gc;
    parser.hadError = false;
    parser.panicMode = false;

    initScanner(&parser.scanner, source);
    Compiler compiler;
    initCompiler(&parser, &compiler, TYPE_SCRIPT);

    advance(&parser);

    // Consume statements
    while(!match(&parser, TOKEN_EOF)) {
        // Ignore lines that are just newlines or semicolons 
        if (match(&parser, TOKEN_NEWLINE) || match(&parser, TOKEN_SEMICOLON)) continue;

        declaration(&parser);
    }

    ObjFunction* function = endCompiler(&parser);
    return parser.hadError ? NULL : function;
}

void markCompilerRoots(Parser* parser) {
    Compiler* compiler = parser->currentCompiler;
     
    while(compiler != NULL) {
        markObject(parser->gc, (Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}