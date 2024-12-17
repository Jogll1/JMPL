#include <stdio.h>
#include <string.h>
#include <wchar.h>

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

static bool isAlpha(int c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 0x03b1 && c <= 0x03c9) || // α to ω
           (c >= 0x0391 && c <= 0x03a9) || // Α to Ω
           (c == '_');
}

static bool isDigit(int c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static int advance() {
    scanner.current++;
    return (int)scanner.current[-1];
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

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
    if(scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
    // Switch identifier to possibly match it to a keyword
    switch(scanner.start[0]) {
        case 'a': return checkKeyword(1, 2, "nd",  TOKEN_AND);
        case 'd': return checkKeyword(1, 1, "o",   TOKEN_DO);
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'a': return checkKeyword(1, 3, "lse", TOKEN_FALSE);
                    case 'u': return checkKeyword(1, 2, "nc",  TOKEN_FUNCTION);
                }
            }
        case 'i': return checkKeyword(1, 1, "f",  TOKEN_IF);
        case 'l': return checkKeyword(1, 2, "et", TOKEN_LET);
        case 'n': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'o': return checkKeyword(1, 1, "t",   TOKEN_NOT);
                    case 'u': return checkKeyword(1, 3, "ull", TOKEN_NULL);
                }
            }
        case 'o': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'u': return checkKeyword(1, 1, "t", TOKEN_OUT);
                    case 'r': return TOKEN_OR;
                }
            }
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 't': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'h': return checkKeyword(1, 2, "en", TOKEN_THEN);
                    case 'r': return checkKeyword(1, 3, "ue", TOKEN_TRUE);
                }
            }
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
        case 'S': return checkKeyword(1, 2, "um",   TOKEN_SUMMATION);
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
        // Consume '.'
        advance();

        while(isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
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

    if(isAtEnd()) return makeToken(TOKEN_EOF);

    int c = advance();

    if(isAlpha(c)) return identifier();
    if(isDigit(c)) return number();

    switch(c) {
        // Switch single characters
        case '(': return makeToken(TOKEN_LEFT_PAREN); break;
        case ')': return makeToken(TOKEN_RIGHT_PAREN); break;
        case '{': return makeToken(TOKEN_LEFT_BRACE); break;
        case '}': return makeToken(TOKEN_RIGHT_BRACE); break;
        case '[': return makeToken(TOKEN_LEFT_SQUARE); break;
        case ']': return makeToken(TOKEN_RIGHT_SQUARE); break;
        case ',': return makeToken(TOKEN_COMMA); break;
        case '.': return makeToken(TOKEN_DOT); break;
        case '-': return makeToken(TOKEN_MINUS); break;
        case '+': return makeToken(TOKEN_PLUS); break;
        case '/': return makeToken(TOKEN_SLASH); break;
        case '*': return makeToken(TOKEN_ASTERISK); break;
        case '^': return makeToken(TOKEN_CARET); break;
        case '%': return makeToken(TOKEN_PERCENT); break;
        case ';': return makeToken(TOKEN_SEMICOLON); break;
        case '|': return makeToken(TOKEN_PIPE); break;
        case 0x2208: return makeToken(TOKEN_IN); break; // '∈'
        case 0x2227: return makeToken(TOKEN_AND); break; // '∧'
        case 0x2228: return makeToken(TOKEN_OR); break; // '∨'
        case '#': return makeToken(TOKEN_HASHTAG); break;
        case 0x2260: return makeToken(TOKEN_NOT_EQUAL); break; // '≠'
        case 0x2264: return makeToken(TOKEN_GREATER_EQUAL); break; // '≤'
        case 0x2265: return makeToken(TOKEN_LESS_EQUAL); break; // '≥'
        case 0x2192: return makeToken(TOKEN_MAPS_TO); break; // '→'
        case 0x21d2: return makeToken(TOKEN_IMPLIES); break; // '⇒'
        case 0x2211: return makeToken(TOKEN_SUMMATION); break; // '∑'
        // Switch one or two character symbols
        case ':': 
            return makeToken(match('=') ? TOKEN_ASSIGN : TOKEN_COLON);
            break;
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
            break;
        case 0x00AC: // '¬'
            return makeToken(match('=') ? TOKEN_NOT_EQUAL : TOKEN_NOT);
            break;
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
            break;
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
            break;
        // Literals
        case '"': return string();
    }

    // return errorToken("Unexpected character: '" + c + "'");
    return errorToken("Unexpected character.");
}