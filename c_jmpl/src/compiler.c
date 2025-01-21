#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
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

// Note: Remove globals eventually
Parser parser;
Compiler* current = NULL;

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void errorAt(Token* token, const unsigned char* message) {
    if(parser.panicMode) return;
    parser.panicMode = true;
    
    fprintf(stderr, "[line %d] " RED "Error" RESET, token->line);

    if(token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s.\n", message);
    exit(70); // TEMPORARY HACK TO ALLOW PROGRAM TO EXIT WHEN THERE'S A COMPILER ERROR
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

    while(true) {
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

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if(offset > UINT16_MAX) error("Loop body too large");

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

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = currentChunk()->count - offset - 2;

    if(jump > UINT16_MAX) {
        error("Too much code to jump over");
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
    compiler->function = newFunction();
    current = compiler;

    if(type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
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
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
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

    while(current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if(current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
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

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for(int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if(upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if(upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;

    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if(compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if(local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if(upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
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
    local->isCaptured = false;
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

static uint8_t parseVariable(const unsigned char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if(current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialised() {
    if(current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if(current->scopeDepth > 0) {
        markInitialised();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if(!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if(argCount == 255) {
                error("Can't have more than 255 arguments");
            }
            argCount++;
        } while(match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
    return argCount;
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
        case TOKEN_MOD:           emitByte(OP_MOD);           break;
        default: return;
    }
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
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

static void and_(bool canAssign) {
    // If left operand is false (currently on stack), emit call to jump to the end
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    
    // Pop left operand if true and evaluate right operand
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    
    patchJump(endJump);
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
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
    } else if((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
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

static void sequenceOp(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    consume(TOKEN_LEFT_PAREN, "Expected '(' before upper bound expression");

    // Parse upperbound expression and store it in a local variable?
    expression();

    consume(TOKEN_COMMA, "Expected ',' after upper bound expression");

    beginScope();
    
    // Parse lowerbound expression (only assignments or declarations)
    // -- for now only declarations --
    uint8_t var = parseVariable("Expected variable name");
    Token name = parser.previous;

    if(match(TOKEN_EQUAL)) {
        // Declare a variable with an expression as its initial value
        expression();
    } else {
        // Error if no assignment
        error("Lower bound expression must be initialised");
    }

    defineVariable(var);
    // -------------------------------

    consume(TOKEN_RIGHT_PAREN, "Expected ')' lower bound expression");

    // ---- Loop operation ----
    expression();
    // int exitJump = -1;
    
    // // Exit conditions (i > n)
    // emitByte(OP_POP); // Eek
    // namedVariable(name, false); // Load i
    // emitByte(OP_LESS_EQUAL); // Check if i <= n

    // // Jump if false
    // exitJump = emitJump(OP_JUMP_IF_FALSE);
    // emitByte(OP_POP); // Pop condition result

    // // Implicit increment of lower bound
    // int incrementStart = currentChunk()->count;
    // emitConstant(NUMBER_VAL(1));
    // // Increment lower bound variable
    // namedVariable(name, false); // Load i
    // emitByte(OP_ADD);
    // int arg = resolveLocal(current, &name);
    // emitBytes(OP_SET_LOCAL, (uint8_t)arg);
    // emitByte(OP_POP); // Pop result?

    // // Parse summand
    // // TODO: actually add stuff
    // parsePrecedence(PREC_SEQUENCE_OP);
    // emitByte(OP_SUMMATION);

    // emitLoop(incrementStart);

    // // Patch jump
    // if(exitJump != -1) {
    //     patchJump(exitJump);
    //     emitByte(OP_POP); // Pop condition result
    // }
    // ------------------------
    
    emitByte(OP_NULL); // TEMPORARY: Trying to fix bug where result is popped
    endScope();
}

ParseRule rules[] = {
    // Rules for parsing expressions
    // Token name            prefix      infix   precedence
    [TOKEN_LEFT_PAREN]    = {grouping,   call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,       NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,       NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,       NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,       NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,       NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,      binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,       binary, PREC_TERM},
    [TOKEN_SLASH]         = {NULL,       binary, PREC_FACTOR},
    [TOKEN_ASTERISK]      = {NULL,       binary, PREC_FACTOR},
    [TOKEN_CARET]         = {NULL,       binary, PREC_EXPONENT},
    [TOKEN_MOD]           = {NULL,       binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,       NULL,   PREC_NONE},
    [TOKEN_COLON]         = {NULL,       NULL,   PREC_NONE},
    [TOKEN_PIPE]          = {NULL,       NULL,   PREC_NONE},
    [TOKEN_IN]            = {NULL,       NULL,   PREC_NONE},
    [TOKEN_HASHTAG]       = {NULL,       NULL,   PREC_NONE},
    [TOKEN_EQUAL]         = {NULL,       NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,       binary, PREC_EQUALITY},
    [TOKEN_ASSIGN]        = {NULL,       NULL,   PREC_NONE},
    [TOKEN_NOT]           = {unary,      NULL,   PREC_NONE},
    [TOKEN_NOT_EQUAL]     = {NULL,       binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,       binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,       binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,       binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,       binary, PREC_COMPARISON},
    [TOKEN_MAPS_TO]       = {NULL,       NULL,   PREC_NONE},
    [TOKEN_IMPLIES]       = {NULL,       NULL,   PREC_NONE},
    [TOKEN_IDENTIFIER]    = {variable,   NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,     NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,     NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,       and_,   PREC_AND},
    [TOKEN_OR]            = {NULL,       or_,    PREC_OR},
    [TOKEN_XOR]           = {NULL,       NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,    NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,    NULL,   PREC_NONE},
    [TOKEN_LET]           = {NULL,       NULL,   PREC_NONE},
    [TOKEN_NULL]          = {literal,    NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,       NULL,   PREC_NONE},
    [TOKEN_THEN]          = {NULL,       NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,       NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,       NULL,   PREC_NONE},
    [TOKEN_DO]            = {NULL,       NULL,   PREC_NONE},
    [TOKEN_SUMMATION]     = {sequenceOp, NULL,   PREC_SEQUENCE_OP},
    [TOKEN_OUT]           = {NULL,       NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,       NULL,   PREC_NONE},
    [TOKEN_FUNCTION]      = {NULL,       NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,       NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,       NULL,   PREC_NONE},
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

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expected '(' after function name");

    if(!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if(current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters");
            }

            uint8_t constant = parseVariable("Expected parameter name");
            defineVariable(constant);
        } while(match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after function parameters");
    consume(TOKEN_EQUAL, "Expected '=' after function signature");

    // Compile the body
    statement();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for(int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void functionDeclaration() {
    uint8_t global = parseVariable("Expected function name");
    markInitialised();
    function(TYPE_FUNCTION);
    defineVariable(global);
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
    if (!current->implicitReturn) emitByte(OP_POP);
}

static void ifStatement() {
    expression();
    consume(TOKEN_THEN, "Expected 'then' after condition");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if(match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

static void outStatement() {
    expression();
    consumeSemicolon();
    emitByte(OP_OUT);
}

static void returnStatement() {
    if(current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code");
    }

    if(match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        expression();
        consumeSemicolon();
        emitByte(OP_RETURN);
    }
}

static void whileStatement() {
    int loopStart = currentChunk()->count;
    expression();
    consume(TOKEN_DO, "Expected 'do' after condition");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
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
    if(match(TOKEN_FUNCTION)) {
        functionDeclaration();
    } else if(match(TOKEN_LET)) {
        letDeclaration();
    } else {
        statement();
    }

    if(parser.panicMode) synchronise();
}

static void statement() {
    current->implicitReturn = false;
    
    if(match(TOKEN_OUT)) {
        outStatement();
    } else if(match(TOKEN_IF)) {
        ifStatement();
    } else if(match(TOKEN_RETURN)) {
        returnStatement();
    } else if(match(TOKEN_WHILE)) {
        whileStatement();
    } else if(match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        // Set the implicit return flag if in a function
        if(current->type == TYPE_FUNCTION) current->implicitReturn = true;

        expressionStatement();
    }
}

ObjFunction* compile(const unsigned char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    
    // Consume statements
    while(!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
     
    while(compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}