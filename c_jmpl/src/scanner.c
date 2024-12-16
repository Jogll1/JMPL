#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if(!isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
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

static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_EOF;
    token.start = message;
    token.length = (int)strlen(message);
    return token;
}

static void skipWhitespace() {
    for(;;) {
        char c = peek();

        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if(peekNext() == '/') {
                    // If it is a comment, keep consuming until end of the line
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if(peekNext() == '*') {
                    // If it is a multi-line comment, keep consuming until closed off
                    while ((peek() != '*' || peekNext() != '/') && !isAtEnd()) {
                        // Increment line counter manually when scanning multi-line comments
                        if(peek() == '\n') scanner.line++;
                        advance();
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token string() {
    while(peek() != '"' && isAtEnd()) {
        if(peek() == '\n') scanner.line++;
        advance();
    }

    if(isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote
    advance();
    return makeToken(TOKEN_STRING);
}

Token scanToken() {
    skipWhitespace();
    scanner.start = scanner.current;

    if(!isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();

    switch(c) {
        // Switch single characters
        case '(': makeToken(TOKEN_LEFT_PAREN); break;
        case ')': makeToken(TOKEN_RIGHT_PAREN); break;
        case '{': makeToken(TOKEN_LEFT_BRACE); break;
        case '}': makeToken(TOKEN_RIGHT_BRACE); break;
        case '[': makeToken(TOKEN_LEFT_SQUARE); break;
        case ']': makeToken(TOKEN_RIGHT_SQUARE); break;
        case ',': makeToken(TOKEN_COMMA); break;
        case '.': makeToken(TOKEN_DOT); break;
        case '-': makeToken(TOKEN_MINUS); break;
        case '+': makeToken(TOKEN_PLUS); break;
        case '^': makeToken(TOKEN_CARET); break;
        case '%': makeToken(TOKEN_PERCENT); break;
        case ';': makeToken(TOKEN_SEMICOLON); break;
        case '|': makeToken(TOKEN_PIPE); break;
        case '∈': makeToken(TOKEN_IN); break;
        case '∧': makeToken(TOKEN_AND); break;
        case '∨': makeToken(TOKEN_OR); break;
        case '#': makeToken(TOKEN_HASHTAG); break;
        case '≠': makeToken(TOKEN_NOT_EQUAL); break;
        case '≥': makeToken(TOKEN_GREATER_EQUAL); break;
        case '≤': makeToken(TOKEN_LESS_EQUAL); break;
        case '→': makeToken(TOKEN_MAPS_TO); break;
        case '⇒': makeToken(TOKEN_IMPLIES); break;
        case '∑': makeToken(TOKEN_SUMMATION); break;
        // Switch one or two character symbols
        case ':': 
            makeToken(match('=') ? TOKEN_ASSIGN : TOKEN_COLON);
            break;
        case '=':
            makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
            break;
        case '¬':
            makeToken(match('=') ? TOKEN_NOT_EQUAL : TOKEN_NOT);
            break;
        case '>':
            makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
            break;
        case '<':
            makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
            break;
        // Literals
        case '"': return string();
    }

    // return errorToken("Unexpected character: '" + c + "'");
    return errorToken("Unexpected character.");
}