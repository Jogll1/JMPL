#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

// Precedence order lowest to highest
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // :=
    PREC_SEQUENCE_OP, // Sum
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

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

// Note: Remove globals eventually
Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk() {
    return compilingChunk;
}

static void errorAt(Token* token, const unsigned char* message) {
    if(parser.panicMode) return;
    parser.panicMode = true;
    
    fprintf(stderr, "[line %d] Error", token->line);

    if(token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s.\n", message);
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

    for(;;) {
        parser.current = scanToken();
        // printf("parsed %.*s, %d\n", parser.current.length, parser.current.start, parser.current.type);
        if(parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const unsigned char* message) {
    if(parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static void consumeSemicolon() {
    consume(TOKEN_SEMICOLON, "Expected ';' after value");
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if(!check(type)) return false;
    advance();

    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);

    if(constant > UINT8_MAX) {
        error("Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void initCompiler(Compiler* compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void endCompiler() {
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    if(!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while(current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}

// Function declarations 
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precendence);

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if(a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for(int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];

        if(identifiersEqual(name, &local->name)) {
            if(local->depth == -1) {
                error("Can't read local variable in its own initialiser");
            }

            return i;
        }
    }

    return -1;
}

static void addLocal(Token name) {
    if(current->localCount == UINT8_COUNT) {
        error("Too many local variables in current scope");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

static void declareVariable() {
    if(current->scopeDepth == 0) return;

    Token* name = &parser.previous;

    for(int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if(local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if(identifiersEqual(name, &local->name)) {
            error("Variable with this identifier already defined in this scope");
        }
    }

    addLocal(*name);
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch(operatorType) {
        case TOKEN_NOT_EQUAL:     emitByte(OP_NOT_EQUAL);     break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL);         break;
        case TOKEN_GREATER:       emitByte(OP_GREATER);       break;
        case TOKEN_GREATER_EQUAL: emitByte(OP_GREATER_EQUAL); break;
        case TOKEN_LESS:          emitByte(OP_LESS);          break;
        case TOKEN_LESS_EQUAL:    emitByte(OP_LESS_EQUAL);    break;
        case TOKEN_PLUS:          emitByte(OP_ADD);           break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT);      break;
        case TOKEN_ASTERISK:      emitByte(OP_MULTIPLY);      break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE);        break;
        case TOKEN_CARET:         emitByte(OP_EXPONENT);      break;
        default: return;
    }
}

static void literal(bool canAssign) {
    switch(parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NULL:  emitByte(OP_NULL);  break;
        case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
        default: return;
    }
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void number(bool canAssign) {
    // Convert string to double
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
    // Copy the string from the source, +1 and -2 trim quotation marks
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);

    if(arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    
    if(canAssign && match(TOKEN_ASSIGN)) {
        // Assign to variable if found ':='
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // Compile the operand
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction
    switch(operatorType) {
        case TOKEN_NOT:   emitByte(OP_NOT);    break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return;
    }
}

ParseRule rules[] = {
    // Rules for parsing expressions
    // Token name            prefix    infix   precedence
    [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_ASTERISK]      = {NULL,     binary, PREC_FACTOR},
    [TOKEN_CARET]         = {NULL,     binary, PREC_EXPONENT},
    [TOKEN_PERCENT]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COLON]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_PIPE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IN]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_HASHTAG]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_ASSIGN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NOT]           = {unary,    NULL,   PREC_NONE},
    [TOKEN_NOT_EQUAL]     = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_MAPS_TO]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IMPLIES]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_XOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_LET]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NULL]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THEN]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DO]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUMMATION]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_OUT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUNCTION]      = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parsePrecedence(Precedence precendence) {
    advance();

    // Get which function to call for the prefix as the first token of an expression is always a prefix (inc. numbers)
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if(prefixRule == NULL) {
        error("Expected expression");
        return;
    }
    
    // Call the prefix function
    bool canAssign = precendence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while(precendence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;

        // Call the function for the infix operation
        infixRule(canAssign);
    }

    if(canAssign && match(TOKEN_ASSIGN)) {
        error("Invalid assignment target");
    }
}

static uint8_t parseVariable(const unsigned char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if(current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialised() {
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if(current->scopeDepth > 0) {
        markInitialised();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while(!check(TOKEN_RIGHT_PAREN) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_PAREN, "Expected '}' after block");
}

static void letDeclaration() {
    uint8_t global = parseVariable("Expected variable name");

    if(match(TOKEN_EQUAL)) {
        // Declare a variable with an expression as its initial value
        expression();
    } else {
        // Declare it with a null value
        emitByte(OP_NULL);
    }

    consumeSemicolon();
    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consumeSemicolon();
    emitByte(OP_POP);
}

static void outStatement() {
    expression();
    consumeSemicolon();
    emitByte(OP_OUT);
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
            case TOKEN_OUT:
            case TOKEN_RETURN:
                return;
            default:; // Do nothing
        }
    }

    advance();
}

static void declaration() {
    if(match(TOKEN_LET)) {
        letDeclaration();
    } else {
        statement();
    }

    if(parser.panicMode) synchronise();
}

static void statement() {
    if(match(TOKEN_OUT)) {
        outStatement();
    } else if(match(TOKEN_LEFT_PAREN)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

bool compile(const unsigned char* source, Chunk* chunk) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    
    // Consume statements
    while(!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();

    return !parser.hadError;
}