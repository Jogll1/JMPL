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
#include "unicode.h"

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

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// Note: Remove globals eventually
Parser parser;
Compiler* current = NULL;

static Chunk* currentChunk() {
    return &current->function->chunk;
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

static void errorAt(Token* token, const unsigned char* message) {
    if(parser.panicMode) return;
    parser.panicMode = true;
    
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
    parser.hadError = true;
}

static void error(const unsigned char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const unsigned char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;
    
    while (true) {
        parser.current = scanToken(&parser.scanner);
#ifdef DEBUG_PRINT_TOKENS
        // Debug tokens
        printf("%s", getTokenName(parser.current.type));
        printf("(");
        fprintfRawString(stderr, parser.current.start, parser.current.length);
        printf(") ");
        if (parser.current.type == TOKEN_NEWLINE || parser.current.type == TOKEN_DEDENT || parser.current.type == TOKEN_EOF) printf("\n\n");
#endif

        if(parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
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
static void consume(TokenType type, const unsigned char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

/**
 * @brief Skip zero or more newlines.
 */
static void skipNewlines() {
    while (parser.current.type == TOKEN_NEWLINE) {
        advance();
    }
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();

    return true;
}

/**
 * @brief Consume a statement separator (newline or semicolon).
 */
static void consumeSeparator() {
    // Make sure a seperator separates statements
    if (match(TOKEN_SEMICOLON) || match(TOKEN_NEWLINE)) return;
    
    if (check(TOKEN_INDENT) || check(TOKEN_DEDENT) || check(TOKEN_EOF)) return;
    
    error("Invalid syntax");
}

static void emitByte(uint8_t byte) {
    writeChunk(parser.gc, currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitOpShort(uint8_t byte, uint16_t u16) {
    emitByte(byte);
    emitBytes((uint8_t)(u16 >> 8), (uint8_t)(u16 & 0xFF)); // Split the u16 into two bytes
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("(Internal) Loop body too large");

    emitByte((offset >> 8) & 0xFF);
    emitByte(offset & 0xFF);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);

    // Jump over offset
    emitByte(0xFF);
    emitByte(0xFF);

    return currentChunk()->count - 2;
}

static void emitReturn() {
    // Implicitly returns null if function returns nothing
    if (!current->implicitReturn || current->type != TYPE_FUNCTION) {
        emitByte(OP_NULL);
    }

    emitBytes(OP_RETURN, current->implicitReturn);
}

static uint16_t makeConstant(Value value) {
    int constant = findConstant(currentChunk(), value);
    if (constant == -1) {
        constant = addConstant(parser.gc, currentChunk(), value);
    }

    if (constant > UINT16_MAX) {
        error("(Internal) Too many constants in one chunk");
        return 0;
    }

    return (uint16_t)constant;
}

static void emitConstant(Value value) {
    emitOpShort(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("(Internal) Too much code to jump over");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xFF;
    currentChunk()->code[offset + 1] = jump & 0xFF;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->implicitReturn = false;
    compiler->function = newFunction(parser.gc);
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.gc, parser.previous.start, parser.previous.length);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL ? (const char*)function->name->utf8 : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

// Function declarations 
static void expression(bool ignoreNewlines);
static void statement(bool blockAllowed, bool ignoreSeparator);
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precendence, bool ignoreNewlines);

static uint16_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(parser.gc, name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];

        if (identifiersEqual(name, &local->name)) {
            if(local->depth == -1) {
                error("Can't read local variable in its own initialiser");
            }

            return i;
        }
    }

    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("(Internal) Too many closure variables in function");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;

    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if(compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("(Internal) Too many local variables in current scope");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

/**
 * @brief Declare a local variable.
 * 
 * @return Whether the variable is already defined
 */
static void declareVariable() {
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];

        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Variable with this identifier already defined in this scope");
        }
    }

    addLocal(*name);
}

/**
 * @brief Parse a variable.
 * 
 * @param errorMessage Error to return if an identifier is not found
 * @return             The index of a variable if it is a global
 * 
 * Note: local variables live on the stack so are accessed via a stack index
 */
static uint16_t parseVariable(const unsigned char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialised() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint16_t global) {
    if (current->scopeDepth > 0) {
        markInitialised();
        return;
    }

    emitOpShort(OP_DEFINE_GLOBAL, global);
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
static uint8_t syntheticLocal(OpCode code, const char* name) {
    emitByte(code);
    
    uint8_t varSlot = current->localCount;
    addLocal(syntheticToken(name));
    markInitialised();
    emitBytes(OP_SET_LOCAL, varSlot);

    return varSlot;
}

/**
 * @brief Parse a generator in the form 'x in Set'.
 * 
 * @return The slot of the generator
 * 
 * Pushes: a local variable, a null value initialiser, and the set
 */
static uint8_t parseGenerator() {
    // Parse the local variable that will be the generator
    uint8_t localVarSlot = current->localCount;
    consume(TOKEN_IDENTIFIER, "Expected identifier");
    
    // === Check if name is already defined ===
    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];

        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            return 0;
        }
    }
    // ========================================

    addLocal(*name);

    emitByte(OP_NULL); // Set it to null initially
    defineVariable(localVarSlot);

    consume(TOKEN_IN, "Expected 'in' or '∈' after identifier");

    // Push the set (to create an iterator)
    expression(false);

    return localVarSlot;
}

/**
 * @brief Creates and closes a new compiler around a c function call.
 * 
 * @param type The type of function to create
 * @param f    The c function to call to compile the inner part of the function
 */
static void functionWrapper(FunctionType type, void (*f)()) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    (*f)();

    ObjFunction* function = endCompiler();
    emitOpShort(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            skipNewlines();
            expression(true);

            if(argCount == 255) {
                error("(Internal) Can't have more than 255 arguments");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }

    skipNewlines();

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
    return argCount;
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1), false);

    switch (operatorType) {
        case TOKEN_NOT_EQUAL:     emitByte(OP_NOT_EQUAL);      break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL);          break;
        case TOKEN_GREATER:       emitByte(OP_GREATER);        break;
        case TOKEN_GREATER_EQUAL: emitByte(OP_GREATER_EQUAL);  break;
        case TOKEN_LESS:          emitByte(OP_LESS);           break;
        case TOKEN_LESS_EQUAL:    emitByte(OP_LESS_EQUAL);     break;
        case TOKEN_PLUS:          emitByte(OP_ADD);            break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT);       break;
        case TOKEN_ASTERISK:      emitByte(OP_MULTIPLY);       break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE);         break;
        case TOKEN_CARET:         emitByte(OP_EXPONENT);       break;
        case TOKEN_MOD:           emitByte(OP_MOD);            break;
        case TOKEN_IN:            emitByte(OP_SET_IN);         break;
        case TOKEN_INTERSECT:     emitByte(OP_SET_INTERSECT);  break;
        case TOKEN_UNION:         emitByte(OP_SET_UNION);      break;
        case TOKEN_BACK_SLASH:    emitByte(OP_SET_DIFFERENCE); break;
        case TOKEN_SUBSET:        emitByte(OP_SUBSET);         break;
        case TOKEN_SUBSETEQ:      emitByte(OP_SUBSETEQ);       break;
        default: return;
    }
}

/**
 * @brief Creates an implicit multiplication.
 * 
 * Called when an identifier comes directly after another token (i.e. 2n).
 * Compiles the identifier then emits a multiply.
 */
static void implMult(bool canAssign) {
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if (prefixRule == NULL) {
        error("Expected expression");
        return;
    }
    
    prefixRule(canAssign);

    emitByte(OP_MULTIPLY);
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void subscript(bool canAssign) {
    expression(true);

    // if (match(TOKEN_COLON)) {
    //     expression(true);
    // } else {

    // }

    consume(TOKEN_RIGHT_SQUARE, "Expected ']' after expression");

    emitByte(OP_SUBSCRIPT);
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NULL:  emitByte(OP_NULL);  break;
        case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
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
static void tuple() {
    if (check(TOKEN_ELLIPSIS)) {
        // Omission operation without 'next'
        advance();
        expression(true);
        emitBytes(OP_TUPLE_OMISSION, 0);
    } else {
        if (match(TOKEN_COMMA)) {
            if (check(TOKEN_RIGHT_PAREN)) {
                // 1-tuple
                emitBytes(OP_CREATE_TUPLE, 1);
                return;
            }

            expression(true);

            if (check(TOKEN_ELLIPSIS)) {
                // Omission operation with 'next'
                advance();
                expression(true);
                emitBytes(OP_TUPLE_OMISSION, 1);
            } else {
                // Normal tuple construction
                int count = 2;
                while (match(TOKEN_COMMA)) {
                    expression(true);
                    if (count < UINT8_MAX) {
                        count++;
                    } else {
                        error("(Internal) Can't have more than 255 elements in a tuple literal");
                    }
                }

                emitBytes(OP_CREATE_TUPLE, count);
            }
        }
    }
}

static void grouping(bool canAssign) {
    // If its an empty parentheses, make an empty tuple
    if (check(TOKEN_RIGHT_PAREN)) {
        advance();
        emitBytes(OP_CREATE_TUPLE, 0);
        return;
    }

    expression(true);

    // Could be a tuple
    tuple();  

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
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

static bool parseSetBuilderGenerator(int oldCount, uint8_t** generatorSlots, uint8_t** iteratorSlots, uint8_t** loopStarts, uint8_t** exitJumps) {
    Parser temp = parser;
    // Check if its a generator 'x ∈'
    bool isGenerator = match(TOKEN_IDENTIFIER) && match(TOKEN_IN);
    parser = temp;

    if (isGenerator) {
        uint8_t generatorSlot = parseGenerator();
        if (generatorSlot == 0) {
            parser = temp;
            return false; // Return false if already defined
        }

        uint8_t iteratorSlot = syntheticLocal(OP_CREATE_ITERATOR, "@iter");

        growArray(generatorSlots, oldCount, generatorSlot);
        growArray(iteratorSlots, oldCount, iteratorSlot);

        // Generate loop
        int loopStart = currentChunk()->count;
        emitBytes(OP_GET_LOCAL, iteratorSlot);
        emitByte(OP_ITERATE);

        int exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Pop check

        emitBytes(OP_SET_LOCAL, generatorSlot);
        emitByte(OP_POP);

        growArray(loopStarts, oldCount, loopStart);
        growArray(exitJumps, oldCount, exitJump);

        return true;
    }

    return false;
}

/**
 * @brief Parse a set builder.
 */
static void setBuilder() {
    // Set anonymous function name and to implicit return
    current->function->name = copyString(parser.gc, "@setb", 5);
    current->implicitReturn = true;

    // Store an opened set as a local
    uint8_t setSlot = syntheticLocal(OP_SET_CREATE, "@set");

    uint8_t* generatorSlots = NULL;
    uint8_t* iteratorSlots = NULL;
    uint8_t* loopStarts = NULL;
    uint8_t* exitJumps = NULL;
    int generatorCount = 0;

    uint8_t* skipJumps = NULL;
    int skipCount = 0;

    // Check if expression is a generator, otherwise skip to pipe
    Parser initialParser = parser;
    bool hasLHSGenerator = false;
    if (parseSetBuilderGenerator(generatorCount, &generatorSlots, & iteratorSlots, &loopStarts, &exitJumps)) {
        generatorCount++;
        hasLHSGenerator = true;
    } else {
        while (!check(TOKEN_PIPE)) advance();
    }
    consume(TOKEN_PIPE, "Expected '|' after expression or generator");

    // Parse qualifiers (generators and filters)
    bool hasRHS = false;
    do {
        if (check(TOKEN_RIGHT_BRACE)) break;
        hasRHS = true;

        // Check if it is a generator
        if (parseSetBuilderGenerator(generatorCount, &generatorSlots, & iteratorSlots, &loopStarts, &exitJumps)) {
            generatorCount++;
            continue;
        }

        // Not a generator, so a predicate
        expression(false);
        int skipJump = emitJump(OP_JUMP_IF_FALSE_2);
        emitByte(OP_POP);

        growArray(&skipJumps, skipCount, skipJump);
        skipCount++;
    } while (match(TOKEN_COMMA));

    if (!hasRHS) errorAtCurrent("Set-builder must have at one qualifier");
    if (generatorCount == 0) errorAtCurrent("Set-builder must have at least one generator");
    Parser endParser = parser;
    parser = initialParser;

    // Load set and insert expression
    emitBytes(OP_GET_LOCAL, setSlot);
    if (hasLHSGenerator) {
        emitBytes(OP_GET_LOCAL, generatorSlots[0]);
    } else {
        expression(false);
    }
    emitBytes(OP_SET_INSERT, 1); // This seems to update without OP_SET_LOCAL - pointer fault?
    emitByte(OP_POP);

    // Patch jumps and emit loops
    for (int i = skipCount - 1; i >= 0; i--) {
        patchJump(skipJumps[i]);
    }

    for (int i = generatorCount - 1; i >= 0; i--) {
        emitLoop(loopStarts[i]);
        patchJump(exitJumps[i]);
        emitByte(OP_POP);
    }
    
    parser = endParser;
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after set-builder");
    emitBytes(OP_GET_LOCAL, setSlot); // Push the completed set
    emitByte(OP_STASH);

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
static bool isSetBuilder() {
    Parser initialParser = parser;
    // Currently in an open brace, braceDepth should end at 1 or less in case of nested sets
    int braceDepth = 1; 

    // Find the pipe (to show we are in a set builder)
    while (!check(TOKEN_PIPE) && !check(TOKEN_EOF)) {
        if (check(TOKEN_LEFT_BRACE)) braceDepth++;
        if (check(TOKEN_RIGHT_BRACE)) braceDepth--;
        if (braceDepth == 0) break;

        advance();
    }

    bool isBuilder = check(TOKEN_PIPE);
    parser = initialParser;
    if (!isBuilder) return false;

    // Call set builder and implicitly return its value
    functionWrapper(TYPE_FUNCTION, setBuilder);
    emitBytes(OP_CALL, 0);
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
static void set(bool canAssign) {
    if (!check(TOKEN_RIGHT_BRACE)) {
        // Check if its a set builder
        if (isSetBuilder()) return;
        
        emitByte(OP_SET_CREATE);
        expression(true);

        if (check(TOKEN_ELLIPSIS)) {
            // Omission operation without 'next'
            advance();
            expression(true);
            emitBytes(OP_SET_OMISSION, 0);
        } else {
            if (match(TOKEN_COMMA)) {
                expression(true);

                if (check(TOKEN_ELLIPSIS)) {
                    // Omission operation with 'next'
                    advance();
                    expression(true);
                    emitBytes(OP_SET_OMISSION, 1);
                } else {
                    // Normal set construction
                    emitBytes(OP_SET_INSERT, 2);

                    int count = 0;
                    while (match(TOKEN_COMMA)) {
                        expression(true);
                        count++;
                    }
                    if (count > 0) emitBytes(OP_SET_INSERT, count);
                }
            } else {
                // Singleton set
                emitBytes(OP_SET_INSERT, 1);
            }
        }
    } else {
        // Empty set
        emitByte(OP_SET_CREATE);
    }
    
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after set literal");
}
// ----------------------

static void number(bool canAssign) {
    // Convert string to double
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void and_(bool canAssign) {
    // If left operand is false (currently on stack), emit call to jump to the end
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    
    // Pop left operand if true and evaluate right operand
    emitByte(OP_POP);
    parsePrecedence(PREC_AND, false);
    
    patchJump(endJump);
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR, false);
    patchJump(endJump);
}

static void compileString(unsigned char* start, size_t length) {
    // If a backslash is next
    // if (peek(scanner) == '\\') {
    //     unsigned char c = peek(scanner);
    //     if (0 <= c && c <= 8) {
    //         // Octal
    //     } else if (c == 'x') {
    //         // Hex
    //     } else if (c == 'u') {
    //         // Unicode < 10000
    //     } else if (c == 'U') {
    //         // Unicode >= 10000
    //     } else {
    //         // Simple
    //     }
    // }
}

/**
 * @brief Convert escape sequences into escape characters in a string of UTF-8 bytes.
 * 
 * @param output An empty output string
 * @param chars  The first character of the string
 * @param length The length of the string
 * @returns      The size of the new string
 */
static size_t decodeEscapeString(unsigned char* output, const unsigned char* chars, size_t length) {
    size_t outputLength = 0;

    size_t i = 0;
    while (i < length) {
        if (chars[i] != '\\') {
            output[outputLength++] = chars[i++];
            continue;
        }

        if (i + 1 >= length) {
            errorAtCurrent("Incomplete escape sequence");
        }

        unsigned char escChar = chars[++i]; // Char after '\'
        EscapeType type = getEscapeType(escChar);

        if (type == ESC_SIMPLE) {
            output[outputLength++] = decodeSimpleEscape(escChar);
            i++;
        } else if (type == ESC_HEX || type == ESC_UNICODE || type == ESC_UNICODE_LG) {
            size_t hexCount = type; // Type is encoded as the number of hex digits
            if (i + hexCount >= length) {
                errorAtCurrent("Incomplete hex/unicode escape sequence");
            }

            uint32_t codePoint = 0;
            for (size_t j = 0; j < hexCount; j++) {
                unsigned char c = chars[++i];
                if (!isHex(c)) {
                    errorAtCurrent("Invalid hex digit in escape");
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
            errorAtCurrent("Unknown escape sequence");
        }
    }
    
    return outputLength;
}

static void character(bool canAssign) {
    // Copy the char bytes from the source, +1 and -2 to trim quotation mark
    size_t length = parser.previous.length - 2;
    unsigned char decoded[length];
    size_t newLength = decodeEscapeString(decoded, parser.previous.start + 1, length);

    uint32_t value = utf8ToUnicode(decoded, newLength);

    if (value > UNICODE_MAX) {
        errorAtCurrent("Unsupported character");
    }

    emitConstant(CHAR_VAL(value));
}

static void string(bool canAssign) {
    // Copy the string from the source, +1 and -2 to trim quotation marks
    size_t length = parser.previous.length - 2;
    unsigned char decoded[length];
    size_t newLength = decodeEscapeString(decoded, parser.previous.start + 1, length);

    Value s = OBJ_VAL(copyString(parser.gc, decoded, newLength));

    pushTemp(parser.gc, s);
    emitConstant(s);
    popTemp(parser.gc);
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_ASSIGN)) {
        expression(false);

        if (setOp == OP_SET_GLOBAL) {
            emitOpShort(setOp, (uint16_t)arg);
        } else {
            emitBytes(setOp, (uint8_t)arg);
        }
    } else {
        if (getOp == OP_GET_GLOBAL) {
            emitOpShort(getOp, (uint16_t)arg);
        } else {
            emitBytes(getOp, (uint8_t)arg);
        }
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // Compile the operand
    parsePrecedence(PREC_UNARY, false);

    // Emit the operator instruction
    switch (operatorType) {
        case TOKEN_NOT:     emitByte(OP_NOT);    break;
        case TOKEN_MINUS:   emitByte(OP_NEGATE); break;
        case TOKEN_PLUS:    break;
        case TOKEN_HASHTAG: emitByte(OP_SIZE);   break;
        case TOKEN_ARB:     emitByte(OP_ARB);   break;
        default: return;
    } 
}

static void quantifier() {
    // Set anonymous function name and to implicit return
    current->function->name = copyString(parser.gc, "@quan", 5);
    current->implicitReturn = true;

    TokenType operatorType = parser.previous.type;

    // === FOR LOAN CODE ===
    uint8_t loopVarSlot = parseGenerator();
    if (loopVarSlot == 0) error("Variable with this identifier already defined in this scope");
    uint8_t iteratorSlot = syntheticLocal(OP_CREATE_ITERATOR, "@iter");

    int loopStart = currentChunk()->count;

    // Load and iterate the iterator
    emitBytes(OP_GET_LOCAL, iteratorSlot);
    emitByte(OP_ITERATE); // Push next value + bool for if there is a current value

    // If no current value -> jump to after-loop
    int loopEnd = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Pop check

    // If loop is fine, set iterative variable
    emitBytes(OP_SET_LOCAL, loopVarSlot);
    emitByte(OP_POP);
    // =====================

    // Predicate
    consume(TOKEN_PIPE, "Expected pipe after generator of quantifier");
    expression(false);

    // Invert expression for exists and some
    if (operatorType == TOKEN_EXISTS || operatorType == TOKEN_SOME) {
        emitByte(OP_NOT);
    }

    int loopEarlyExit = emitJump(OP_JUMP_IF_FALSE_2);
    emitByte(OP_POP);

    emitLoop(loopStart);

    // Early exit
    patchJump(loopEarlyExit);

    if (operatorType == TOKEN_SOME) {
        emitBytes(OP_GET_LOCAL, loopVarSlot);
    } else {
        emitByte(operatorType == TOKEN_FORALL ? OP_FALSE : OP_TRUE);
    }

    emitByte(OP_STASH);
    emitBytes(OP_RETURN, current->implicitReturn); // Return manually

    // Loop end
    patchJump(loopEnd);
    emitByte(OP_POP);

    if (operatorType == TOKEN_SOME) {
        emitByte(OP_NULL);
    } else {
        emitByte(operatorType == TOKEN_FORALL ? OP_TRUE : OP_FALSE); 
    }

    emitByte(OP_STASH);
}

/**
 * @brief Wrapper for quantifier to call it as an anonymous function.
 */
static void quantAnon(bool canAssign) {
    functionWrapper(TYPE_FUNCTION, quantifier);
    emitBytes(OP_CALL, 0);
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
    [TOKEN_FORALL]        = {quantAnon,  NULL,      PREC_EQUALITY},
    [TOKEN_EXISTS]        = {quantAnon,  NULL,      PREC_EQUALITY},
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
    [TOKEN_IDENTIFIER]    = {variable,   implMult,  PREC_TERM},
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
    [TOKEN_SOME]          = {quantAnon,  NULL,      PREC_EQUALITY},
    [TOKEN_ARB]           = {unary,      NULL,      PREC_UNARY},
    [TOKEN_RETURN]        = {NULL,       NULL,      PREC_NONE},
    [TOKEN_FUNCTION]      = {NULL,       NULL,      PREC_NONE},
    [TOKEN_NEWLINE]       = {NULL,       NULL,      PREC_NONE},
    [TOKEN_INDENT]        = {NULL,       NULL,      PREC_NONE},
    [TOKEN_DEDENT]        = {NULL,       NULL,      PREC_NONE},
    [TOKEN_ERROR]         = {NULL,       NULL,      PREC_NONE},
    [TOKEN_EOF]           = {NULL,       NULL,      PREC_NONE},
};

static void parsePrecedence(Precedence precendence, bool ignoreNewlines) {
    advance();

    // Ignore newline tokens if the flag is set
    if (ignoreNewlines) skipNewlines();

    // Get which function to call for the prefix as the first token of an expression is always a prefix (inc. numbers)
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if (prefixRule == NULL) {
        error("Expected expression");
        return;
    }
    
    // Call the prefix function
    bool canAssign = precendence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);
    
    while (precendence <= getRule(parser.current.type)->precedence) {
        advance();

        // Ignore newline tokens if the flag is set
        if (ignoreNewlines) skipNewlines();
        ParseFn infixRule = getRule(parser.previous.type)->infix;

        if (infixRule == NULL) {
            error("Invalid syntax");
            return;
        }

        // Call the function for the infix operation
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_ASSIGN)) {
        error("Invalid assignment target");
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
static void expression(bool ignoreNewlines) {
    parsePrecedence(PREC_ASSIGNMENT, ignoreNewlines);
}

static void block() {
    skipNewlines();

    while (!check(TOKEN_DEDENT) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_DEDENT, "Expected 'DEDENT' after block");
}

static void function() {
    consume(TOKEN_LEFT_PAREN, "Expected '(' after function name");

    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if(current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters");
            }

            uint16_t constant = parseVariable("Expected parameter name");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after function parameters");
    consume(TOKEN_EQUAL, "Expected '=' after function signature");

    // Compile the body
    skipNewlines();
    statement(true, false);
}

static void functionDeclaration() {
    uint16_t global = parseVariable("Expected function name");
    markInitialised();
    functionWrapper(TYPE_FUNCTION, function);
    defineVariable(global);
}

static void letDeclaration() {
    uint16_t global = parseVariable("Expected variable name");

    if (match(TOKEN_EQUAL)) {
        // Declare a variable with an expression as its initial value
        expression(false);
    } else {
        // Declare it with a null value
        emitByte(OP_NULL);
    }

    defineVariable(global);

    consumeSeparator();
}

static void expressionStatement() {
    expression(false);
    emitByte(current->implicitReturn ? OP_STASH : OP_POP);
}

static void ifStatement() {
    expression(false);
    skipNewlines();
    consume(TOKEN_THEN, "Expected 'then' after condition");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement(true, true);

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    skipNewlines(); // Skip newlines to search for else
    if(match(TOKEN_ELSE)) statement(true, false);
    skipNewlines();
    
    patchJump(elseJump);
}

static void returnStatement() {
    if(current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code");
    }

    if(match(TOKEN_SEMICOLON) || match(TOKEN_NEWLINE)) { // Interesting
        emitReturn();
    } else {
        expression(false);
        emitBytes(OP_RETURN, 0);
    }
}

static void whileStatement() {
    int loopStart = currentChunk()->count;
    expression(false);
    consume(TOKEN_DO, "Expected 'do' after condition");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Pop condition to check
    skipNewlines();
    statement(true, false);
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void forStatement() {
    beginScope();

    uint8_t loopVarSlot = parseGenerator();
    if (loopVarSlot == 0) error("Variable with this identifier already defined in this scope");
    uint8_t iteratorSlot = syntheticLocal(OP_CREATE_ITERATOR, "@iter");

    int loopStart = currentChunk()->count;

    // Load and iterate the iterator
    emitBytes(OP_GET_LOCAL, iteratorSlot);
    emitByte(OP_ITERATE); // Push next value + bool for if there is a current value

    // If no current value -> jump to after-loop
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Pop check
    
    // If loop is fine, set iterative variable
    emitBytes(OP_SET_LOCAL, loopVarSlot);
    emitByte(OP_POP);

    // Compile optional predicate
    if (match(TOKEN_PIPE)) {
        expression(false);
        consume(TOKEN_DO, "Expected expression");

        // Skip statement if predicate is false
        int skipJump = emitJump(OP_JUMP_IF_FALSE_2);
        emitByte(OP_POP); // Pop predicate

        statement(true, false);

        // Loop and patch
        patchJump(skipJump);

    } else {
        consume(TOKEN_DO, "Expected expression");
        statement(true, false);
    }

    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
    endScope();
}

static void synchronise() {
    parser.panicMode = false;

    while(parser.current.type != TOKEN_EOF) {
        if(parser.previous.type == TOKEN_SEMICOLON) return;
        switch(parser.current.type) {
            case TOKEN_FUNCTION:
            case TOKEN_LET:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_RETURN:
                return;
            default:; // Do nothing
        }
    }

    advance();
}

static void declaration() {
    if (match(TOKEN_FUNCTION)) {
        functionDeclaration();
    } else if (match(TOKEN_LET)) {
        letDeclaration();
    } else {
        statement(false, false);
    }

    if(parser.panicMode) synchronise();

    skipNewlines();
}

static void statement(bool blockAllowed, bool ignoreSeparator) {
    if(current->type == TYPE_SCRIPT) current->implicitReturn = false;
    
    if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
        if(!ignoreSeparator) consumeSeparator();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if(match(TOKEN_INDENT)) {
        if (!blockAllowed) error("Unexpected indent");

        beginScope();
        block();
        endScope();
    } else {
        // Set the implicit return flag if in a function
        if(current->type == TYPE_FUNCTION) {
            current->implicitReturn = true;
        } 
        expressionStatement();
        if(!ignoreSeparator) consumeSeparator();
    }
}

ObjFunction* compile(GC* gc, const unsigned char* source) {
    parser.gc = gc;
    parser.hadError = false;
    parser.panicMode = false;

    initScanner(&parser.scanner, source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    advance();

    // Consume statements
    while(!match(TOKEN_EOF)) {
        // Ignore lines that are just newlines or semicolons 
        if (match(TOKEN_NEWLINE) || match(TOKEN_SEMICOLON)) continue;

        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
     
    while(compiler != NULL) {
        markObject(parser.gc, (Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}