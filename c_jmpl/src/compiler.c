#include <stdio.h>
#include <stdlib.h>

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

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
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

static void endCompiler() {
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    if(!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

// Function declarations 
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precendence);

static void binary() {
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

static void literal() {
    switch(parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NULL:  emitByte(OP_NULL);  break;
        case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
        default: return;
    }
}

static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void number() {
    // Convert string to double
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string() {
    // Copy the string from the source, +1 and -2 trim quotation marks
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name) {
    uint8_t arg = identifierConstant(&name);
    emitBytes(OP_GET_GLOBAL, arg);
}

static void variable() {
    namedVariable(parser.previous);
}

static void unary() {
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
    prefixRule();

    while(precendence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;

        // Call the function for the infix operation
        infixRule();
    }
}

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static parseVariable(const unsigned char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
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
    } else {
        expressionStatement();
    }
}

bool compile(const unsigned char* source, Chunk* chunk) {
    initScanner(source);
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