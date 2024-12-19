#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const unsigned char* start;
    const unsigned char* current;
    int line;
} Scanner;

Scanner scanner;

void initScanner(const unsigned char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

/**
 * Determine if a character is a UTF-8 and how many bytes it contains
 * 
 * @param byte the first byte of the character
 * @returns    whether the character is a UTF-8 character
 * 
 */
static int utf8ByteCount(unsigned char byte) {
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
    int byteLength = utf8ByteCount(scanner.current[-1]);

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
    token.type = TOKEN_EOF;
    token.start = message;
    token.length = (int)strlen(message);
    return token;
}

static void skipWhitespace() {
    for(;;) {
        unsigned int c = peek();

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

static TokenType checkKeyword(unsigned int start, int length, const unsigned char* rest, TokenType type) {
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
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'u': return checkKeyword(2, 2, "nc",  TOKEN_FUNCTION);
                }
            }
        case 'i': return checkKeyword(1, 1, "f",  TOKEN_IF);
        case 'l': return checkKeyword(1, 2, "et", TOKEN_LET);
        case 'n': 
            if(scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'o': return checkKeyword(2, 1, "t",   TOKEN_NOT);
                    case 'u': return checkKeyword(2, 3, "ull", TOKEN_NULL);
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
        // Consume the '.'
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

    unsigned int c = advance();
    // printf("%d %c\n", c, (char)c);

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
        case 0xE28888: return makeToken(TOKEN_IN); break; // '∈' U+2208, UTF-8: 0xE28888
        case 0xE288A7: return makeToken(TOKEN_AND); break; // '∧' U+2227, UTF-8: 0xE288A7
        case 0xE288A8: return makeToken(TOKEN_OR); break; // '∨' U+2228, UTF-8: 0xE288A8
        case '#': return makeToken(TOKEN_HASHTAG); break;
        case 0xE289A0: return makeToken(TOKEN_NOT_EQUAL); break; // '≠' U+2260, UTF-8: 0xE289A0
        case 0xE289A4: return makeToken(TOKEN_GREATER_EQUAL); break; // '≤' U+2264, UTF-8: 0xE289A4
        case 0xE289A5: return makeToken(TOKEN_LESS_EQUAL); break; // '≥' U+2265, UTF-8: 0xE289A5
        case 0xE28692: return makeToken(TOKEN_MAPS_TO); break; // '→' U+2192, UTF-8: 0xE28692
        case 0xE28792: return makeToken(TOKEN_IMPLIES); break; // '⇒' U+21D2, UTF-8: 0xE28792
        case 0xE28891: return makeToken(TOKEN_SUMMATION); break; // '∑' U+2211, UTF-8: 0xE28891
        // Switch one or two character symbols
        case ':': 
            return makeToken(match('=') ? TOKEN_ASSIGN : TOKEN_COLON);
            break;
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
            break;
        case 0xC2AC: // '¬' U+00AC, UTF-8: 0xC2AC
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