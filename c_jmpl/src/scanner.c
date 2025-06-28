#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#define MAX_INDENT_SIZE 16
#define TOKEN_QUEUE_SIZE 16

// --- Scanner and TokenQueue ---

// Circular queue for pending tokens
typedef struct {
    Token tokens[TOKEN_QUEUE_SIZE];
    int head;
    int tail;
} TokenQueue;

typedef struct {
    const unsigned char* start;
    const unsigned char* current;

    int indentStack[MAX_INDENT_SIZE];
    int* indentTop;
    
    TokenQueue tokenQueue;

    int groupingDepth;

    int line;
} Scanner;

Scanner scanner;

void initScanner(const unsigned char* source) {
    scanner.start = source;
    scanner.current = source;

    scanner.indentStack[0] = 0; // Base indent level
    scanner.indentTop = scanner.indentStack; // Top of indent stack
    
    scanner.tokenQueue.head = 0;
    scanner.tokenQueue.tail = 0;

    scanner.groupingDepth = 0; // Shouldn't create indent tokens when in a grouping

    scanner.line = 1;
}

bool isTokenQueueEmpty(TokenQueue* queue) {
    return queue->head == queue->tail;
}

bool isTokenQueueFull(TokenQueue* queue) {
    return (queue->tail + 1) % TOKEN_QUEUE_SIZE == queue->head;
}

// ------

/**
 * @brief Determine the length of a character in bytes that is encoded with UTF-8.
 * 
 * @param byte The first byte of the character
 * @returns    How long the character's byte sequence is
 */
static int getCharByteCount(unsigned char byte) {
    if(byte < 0x80) {
        return 1; // ASCII
    } else if((byte & 0xE0) == 0xC0) {
        return 2;
    } else if((byte & 0xF0) == 0xE0) {
        return 3;
    } else if((byte & 0xF8) == 0xF0) {
        return 4;
    } else {
        return -1; // Invalid
    }
}

/**
 * @brief Check if a character is a character that can start an identifier.advance
 * 
 * @param c The character to check
 * 
 * An identifier can include: the alphabet (a-z, A-Z), the Greek alphabet (a-ω, Α-Ω), and underscores (_).
 */
static bool isAlpha(unsigned int c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 0xCEB1 && c <= 0xCF89) || // α (U+03B1, UTF-8: 0xCEB1) to ω (U+03C9, UTF-8: 0xCF89)
           (c >= 0xCE91 && c <= 0xCEA9) || // Α (U+0391, UTF-8: 0xCE91) to Ω (U+03A9, UTF-8: 0xCEA9)
           (c == '_');
}

static bool isDigit(unsigned int c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static int advance() {
    scanner.current++;

    // Get the length of the character's byte sequence
    int byteLength = getCharByteCount(scanner.current[-1]);

    // Initialise c as current byte
    int c = (int)scanner.current[-1];
    
    // Loop through the rest of the bytes if needed, increment current, and add to result
    for (int i = 0; i < byteLength - 1; i++)
    {
        scanner.current++;
        c = (c << 8) | (int)scanner.current[-1];
    }

    return c;
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if(isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(unsigned char expected) {
    if(isAtEnd()) return false;
    if(*scanner.current != expected) return false;

    scanner.current++;
    return true;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token errorToken(const unsigned char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

bool enqueueToken(Token token) {
    TokenQueue* queue = &scanner.tokenQueue;

    if (isTokenQueueFull(queue)) return false;

    queue->tokens[queue->tail] = token;
    queue->tail = (queue->tail + 1) % TOKEN_QUEUE_SIZE;
    return true;
}

Token dequeueToken() {
    TokenQueue* queue = &scanner.tokenQueue;

    if (isTokenQueueEmpty(queue)) return errorToken("Token queue empty");

    Token token = queue->tokens[queue->head];
    queue->head = (queue->head + 1) % TOKEN_QUEUE_SIZE;
    return token;
}

static void skipWhitespace() {
    while (true) {
        unsigned int c = peek();

        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    // If it is a comment, keep consuming until end of the line
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if (peekNext() =='*') {
                    advance(); // Skip the '/'
                    advance(); // Skip the '*'
                    // If it is a multi-line comment, keep consuming until closed off
                    while (peek() != '*' && peekNext() != '/' && !isAtEnd()) {
                        // Increment line counter manually when scanning multi-line comments
                        if(peek() == '\n') scanner.line++;
                        advance();
                    }
                    advance(); // Skip the '*'
                    advance(); // Skip the '/'
                } else {
                    return;
                }
                break;
            default: return;
        }
    }
}

static TokenType checkKeyword(unsigned int start, int length, const unsigned char* rest, TokenType type) {
    if(scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
    // Switch identifier to possibly match it to a keyword
    switch(scanner.start[0]) {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'd': return checkKeyword(1, 1, "o", TOKEN_DO);
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'u': return checkKeyword(2, 2, "nc", TOKEN_FUNCTION);
                }
            }
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'l': return checkKeyword(1, 2, "et", TOKEN_LET);
        case 'm': return checkKeyword(1, 2, "od", TOKEN_MOD);
        case 'n': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'o': return checkKeyword(2, 1, "t", TOKEN_NOT);
                    case 'u': return checkKeyword(2, 2, "ll", TOKEN_NULL);
                }
            }
        case 'o': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'u': return checkKeyword(2, 1, "t", TOKEN_OUT);
                    case 'r': return TOKEN_OR;
                }
            }
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 't': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'h': return checkKeyword(2, 2, "en", TOKEN_THEN);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
        case 'S': return checkKeyword(1, 2, "um", TOKEN_SUMMATION);
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while(isAlpha(peek()) || isDigit(peek())) advance();

    return makeToken(identifierType());
}

static Token number() {
    while(isDigit(peek())) advance();

    // Look for fractional part
    if(peek() == '.' && isDigit(peekNext())) {
        // Consume the '.'
        advance();

        while(isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

static Token string() {
    while(peek() != '"' && !isAtEnd()) {
        if(peek() == '\n') scanner.line++;
        advance();
    }

    if(isAtEnd()) return errorToken("Unterminated string.");

    advance(); // The closing quote
    return makeToken(TOKEN_STRING);
}

/**
 * @brief Determine whether a newline character indicates a just a newline, indent or dedent.
 * 
 * @return Whether the process completed. Returning false indicates the queue is full
 */
static bool scanAfterNewline() {
    // Count spaces after line starts
    int currentIndent = 0;
    while (peek() == ' ')
    {
        advance();
        currentIndent++;
    }

    // Skip completely empty lines
    skipWhitespace();
    if (peek() == '\n' || peek() == '\0') {
        return true;
    }

    // Queue relevant tokens
    if (currentIndent > *scanner.indentTop) {
        // Indent - Push a new indent level
        if (scanner.indentTop - scanner.indentStack >= MAX_INDENT_SIZE - 1) {
            return enqueueToken(errorToken("Too many nested indents"));
        }

        *(++scanner.indentTop) = currentIndent;
        return enqueueToken(makeToken(TOKEN_INDENT));
    } else {
        // Dedent - Push dedents until it reaches the current indent
        while (scanner.indentTop > scanner.indentStack && currentIndent < *scanner.indentTop) {
            scanner.indentTop--;

            // Otherwise return a dedent token
            if(!enqueueToken(makeToken(TOKEN_DEDENT))) return false;
        }

        // Error if top of indent stack does not match the current indent
        if(*scanner.indentTop != currentIndent) enqueueToken(errorToken("Unexpected indent"));

        // Return true as there has been no queue overflow
        return true;    
    }
}

/**
 * @brief Enqueue dedent tokens to close opened indents
 * 
 * @return If there is room for all the dedent tokens
 */
static bool flushDedents() {
    while (scanner.indentTop > scanner.indentStack) {
        scanner.indentTop--;

        if (!enqueueToken(makeToken(TOKEN_DEDENT))) {
            return false;
        }   
    }

    return true;
}

Token scanToken() {
    // Return any queued tokens
    if (!isTokenQueueEmpty(&scanner.tokenQueue)) return dequeueToken();

    skipWhitespace();
    scanner.start = scanner.current;

    if(isAtEnd()) {
        // If at end and there are unclosed indents, make dedents
        if(!flushDedents()) return errorToken("Pending token queue full");
        if (!isTokenQueueEmpty(&scanner.tokenQueue)) return dequeueToken();
        
        // Otherwise return an EOF token
        return makeToken(TOKEN_EOF);
    }

    // Handle newlines
    if (peek() == '\n') {
        advance();
        scanner.line++;

        // Only return a token if grouping depth is 0
        if (scanner.groupingDepth == 0) {
            // Enqueue indent or dedent tokens if necessary and possible
            if (!scanAfterNewline()) return errorToken("Pending token queue full");

            // Enqueue a newline (if the token wasn't an indent) then return top of queue
            enqueueToken(makeToken(TOKEN_NEWLINE));
            
            return dequeueToken();
        } else {
            // Re-skip whitespace if in a grouping
            skipWhitespace();
        }
    }

    unsigned int c = advance();

    if(isAlpha(c)) return identifier();
    if(isDigit(c)) return number();

    switch(c) {
        // Switch single characters
        case '(': scanner.groupingDepth++; return makeToken(TOKEN_LEFT_PAREN);
        case ')': scanner.groupingDepth--; return makeToken(TOKEN_RIGHT_PAREN);
        case '{': scanner.groupingDepth++; return makeToken(TOKEN_LEFT_BRACE);
        case '}': scanner.groupingDepth--; return makeToken(TOKEN_RIGHT_BRACE);
        case '[': scanner.groupingDepth++; return makeToken(TOKEN_LEFT_SQUARE);
        case ']': scanner.groupingDepth--; return makeToken(TOKEN_RIGHT_SQUARE);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_ASTERISK);
        case '^': return makeToken(TOKEN_CARET);
        case '%': return makeToken(TOKEN_MOD);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case '|': return makeToken(TOKEN_PIPE);
        case 0xE28888: return makeToken(TOKEN_IN); // '∈' U+2208, UTF-8: 0xE28888
        case 0xE288A7: return makeToken(TOKEN_AND); // '∧' U+2227, UTF-8: 0xE288A7
        case 0xE288A8: return makeToken(TOKEN_OR); // '∨' U+2228, UTF-8: 0xE288A8
        case '#': return makeToken(TOKEN_HASHTAG);
        case 0xE289A0: return makeToken(TOKEN_NOT_EQUAL); // '≠' U+2260, UTF-8: 0xE289A0
        case 0xE289A4: return makeToken(TOKEN_LESS_EQUAL); // '≤' U+2264, UTF-8: 0xE289A4
        case 0xE289A5: return makeToken(TOKEN_GREATER_EQUAL); // '≥' U+2265, UTF-8: 0xE289A5
        case 0xE28692: return makeToken(TOKEN_MAPS_TO); // '→' U+2192, UTF-8: 0xE28692
        case 0xE28792: return makeToken(TOKEN_IMPLIES); // '⇒' U+21D2, UTF-8: 0xE28792
        case 0xE28891: return makeToken(TOKEN_SUMMATION); // '∑' U+2211, UTF-8: 0xE28891
        // Switch one or two character symbols
        case '-': return makeToken(match('>') ? TOKEN_MAPS_TO : TOKEN_MINUS);
        case ':': return makeToken(match('=') ? TOKEN_ASSIGN : TOKEN_COLON); 
        case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case 0xC2AC: return makeToken(match('=') ? TOKEN_NOT_EQUAL : TOKEN_NOT); // '¬' U+00AC, UTF-8: 0xC2AC
        case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        // Literals
        case '"': return string();
    }

    return errorToken("Unexpected character");
}