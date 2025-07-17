#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

void initScanner(Scanner* scanner, const unsigned char* source) {
    scanner->start = source;
    scanner->current = source;

    scanner->indentStack[0] = 0; // Base indent level
    scanner->indentTop = scanner->indentStack; // Top of indent stack
    
    scanner->tokenQueue.head = 0;
    scanner->tokenQueue.tail = 0;

    scanner->groupingDepth = 0; // Shouldn't create indent tokens when in a grouping

    scanner->line = 1;
}

bool isTokenQueueEmpty(TokenQueue* queue) {
    return queue->head == queue->tail;
}

bool isTokenQueueFull(TokenQueue* queue) {
    return (queue->tail + 1) % TOKEN_QUEUE_SIZE == queue->head;
}

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

static bool isAtEnd(Scanner* scanner) {
    return *scanner->current == '\0';
}

static int advance(Scanner* scanner) {
    scanner->current++;

    // Get the length of the character's byte sequence
    int byteLength = getCharByteCount(scanner->current[-1]);

    // Initialise c as current byte
    int c = (int)scanner->current[-1];
    
    // Loop through the rest of the bytes if needed, increment current, and add to result
    for (int i = 0; i < byteLength - 1; i++)
    {
        scanner->current++;
        c = (c << 8) | (int)scanner->current[-1];
    }

    return c;
}

static char peek(Scanner* scanner) {
    return *scanner->current;
}

static char peekNext(Scanner* scanner) {
    if(isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}

static bool match(Scanner* scanner, unsigned char expected) {
    if(isAtEnd(scanner)) return false;
    if(*scanner->current != expected) return false;

    scanner->current++;
    return true;
}

static Token makeToken(Scanner* scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static Token errorToken(Scanner* scanner, const unsigned char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;
    return token;
}

bool enqueueToken(Scanner* scanner, Token token) {
    TokenQueue* queue = &scanner->tokenQueue;

    if (isTokenQueueFull(queue)) return false;

    queue->tokens[queue->tail] = token;
    queue->tail = (queue->tail + 1) % TOKEN_QUEUE_SIZE;
    return true;
}

Token dequeueToken(Scanner* scanner) {
    TokenQueue* queue = &scanner->tokenQueue;

    if (isTokenQueueEmpty(queue)) return errorToken(scanner, "Token queue empty");

    Token token = queue->tokens[queue->head];
    queue->head = (queue->head + 1) % TOKEN_QUEUE_SIZE;
    return token;
}

static void skipWhitespace(Scanner* scanner) {
    while (true) {
        unsigned int c = peek(scanner);

        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) == '/') {
                    // If it is a comment, keep consuming until end of the line
                    while (peek(scanner) != '\n' && !isAtEnd(scanner)) advance(scanner);
                } else if (peekNext(scanner) =='*') {
                    advance(scanner); // Skip the '/'
                    advance(scanner); // Skip the '*'
                    // If it is a multi-line comment, keep consuming until closed off
                    while (peek(scanner) != '*' && peekNext(scanner) != '/' && !isAtEnd(scanner)) {
                        // Increment line counter manually when scanning multi-line comments
                        if(peek(scanner) == '\n') scanner->line++;
                        advance(scanner);
                    }
                    advance(scanner); // Skip the '*'
                    advance(scanner); // Skip the '/'
                } else {
                    return;
                }
                break;
            default: return;
        }
    }
}

static bool isKeyword(Scanner* scanner, unsigned int start, int length, const unsigned char* rest, TokenType type) {
    return scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0;
}

static TokenType checkKeyword(Scanner* scanner, unsigned int start, int length, const unsigned char* rest, TokenType type) {
    if(isKeyword(scanner, start, length, rest, type)) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

/**
 * @brief Switch identifier to possibly match it to a keyword.
 */
static TokenType identifierType(Scanner* scanner) {
    switch(scanner->start[0]) {
        case 'a': return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'd': return checkKeyword(scanner, 1, 1, "o", TOKEN_DO);
        case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f': 
            if(scanner->current - scanner->start > 1) {
                switch(scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(scanner, 2, 2, "nc", TOKEN_FUNCTION);
                }
            }
            break;
        case 'i': 
            if(scanner->current - scanner->start > 1) {
                switch(scanner->start[1]) {
                    case 'f': return checkKeyword(scanner, 2, 0, "", TOKEN_IF);
                    case 'n': 
                        if(scanner->current - scanner->start > 2) {
                            switch(scanner->start[2]) {
                                case 't': return checkKeyword(scanner, 3, 6, "ersect", TOKEN_INTERSECT);
                            }
                        }
                        return checkKeyword(scanner, 2, 0, "", TOKEN_IN);
                }
            }
            break;
        case 'l': return checkKeyword(scanner, 1, 2, "et", TOKEN_LET);
        case 'm': return checkKeyword(scanner, 1, 2, "od", TOKEN_MOD);
        case 'n': 
            if(scanner->current - scanner->start > 1) {
                switch(scanner->start[1]) {
                    case 'o': return checkKeyword(scanner, 2, 1, "t", TOKEN_NOT);
                    case 'u': return checkKeyword(scanner, 2, 2, "ll", TOKEN_NULL);
                }
            }
            break;
        case 'o': 
            if(scanner->current - scanner->start > 1) {
                switch(scanner->start[1]) {
                    case 'u': return checkKeyword(scanner, 2, 1, "t", TOKEN_OUT);
                    case 'r': return checkKeyword(scanner, 2, 0, "", TOKEN_OR);
                }
            }
            break;
        case 'r': return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's':
            if(scanner->current - scanner->start > 1) {
                if (isKeyword(scanner, 1, 7, "ubseteq", TOKEN_SUBSETEQ)) return TOKEN_SUBSETEQ;
                else if (isKeyword(scanner, 1, 5, "ubset", TOKEN_SUBSET)) return TOKEN_SUBSET;
                else return TOKEN_IDENTIFIER;
            }
            break;
        case 't': 
            if(scanner->current - scanner->start > 1) {
                switch(scanner->start[1]) {
                    case 'h': return checkKeyword(scanner, 2, 2, "en", TOKEN_THEN);
                    case 'r': return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'u': return checkKeyword(scanner, 1, 4, "nion", TOKEN_UNION);
        case 'w': return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner* scanner) {
    while(isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);

    return makeToken(scanner, identifierType(scanner));
}

static Token number(Scanner* scanner) {
    while(isDigit(peek(scanner))) advance(scanner);

    // Look for fractional part
    if(peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        // Consume the '.'
        advance(scanner);

        while(isDigit(peek(scanner))) advance(scanner);
    }

    return makeToken(scanner, TOKEN_NUMBER);
}

static Token string(Scanner* scanner) {
    while(peek(scanner) != '"' && !isAtEnd(scanner)) {
        if(peek(scanner) == '\n') scanner->line++;
        advance(scanner);
    }

    if(isAtEnd(scanner)) return errorToken(scanner, "Unterminated string.");

    advance(scanner); // The closing quote
    return makeToken(scanner, TOKEN_STRING);
}

/**
 * @brief Determine whether a newline character indicates a just a newline, indent or dedent.
 * 
 * @return Whether the process completed. Returning false indicates the queue is full
 */
static bool scanAfterNewline(Scanner* scanner) {
    // Count spaces after line starts
    int currentIndent = 0;
    while (peek(scanner) == ' ')
    {
        advance(scanner);
        currentIndent++;
    }

    // Skip completely empty lines
    skipWhitespace(scanner);
    if (peek(scanner) == '\n' || peek(scanner) == '\0') {
        return true;
    }

    // Queue relevant tokens
    if (currentIndent > *scanner->indentTop) {
        // Indent - Push a new indent level
        if (scanner->indentTop - scanner->indentStack >= MAX_INDENT_SIZE - 1) {
            return enqueueToken(scanner, errorToken(scanner, "Too many nested indents"));
        }

        *(++scanner->indentTop) = currentIndent;
        return enqueueToken(scanner, makeToken(scanner, TOKEN_INDENT));
    } else {
        // Dedent - Push dedents until it reaches the current indent
        while (scanner->indentTop > scanner->indentStack && currentIndent < *scanner->indentTop) {
            scanner->indentTop--;

            // Otherwise return a dedent token
            if(!enqueueToken(scanner, makeToken(scanner, TOKEN_DEDENT))) return false;
        }

        // Error if top of indent stack does not match the current indent
        if(*scanner->indentTop != currentIndent) enqueueToken(scanner, errorToken(scanner, "Unexpected indent"));

        // Return true as there has been no queue overflow
        return true;    
    }
}

/**
 * @brief Enqueue dedent tokens to close opened indents
 * 
 * @return If there is room for all the dedent tokens
 */
static bool flushDedents(Scanner* scanner) {
    while (scanner->indentTop > scanner->indentStack) {
        scanner->indentTop--;

        if (!enqueueToken(scanner, makeToken(scanner, TOKEN_DEDENT))) {
            return false;
        }   
    }

    return true;
}

Token scanToken(Scanner* scanner) {
    // Return any queued tokens
    if (!isTokenQueueEmpty(&scanner->tokenQueue)) return dequeueToken(scanner);

    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if(isAtEnd(scanner)) {
        // If at end and there are unclosed indents, make dedents
        if(!flushDedents(scanner)) return errorToken(scanner, "Pending token queue full");
        if (!isTokenQueueEmpty(&scanner->tokenQueue)) return dequeueToken(scanner);
        
        // Otherwise return an EOF token
        return makeToken(scanner, TOKEN_EOF);
    }

    // Handle newlines
    if (peek(scanner) == '\n') {
        advance(scanner);
        scanner->line++;

        // Only return a token if grouping depth is 0
        if (scanner->groupingDepth == 0) {
            // Enqueue indent or dedent tokens if necessary and possible
            if (!scanAfterNewline(scanner)) return errorToken(scanner, "Pending token queue full");

            // Enqueue a newline (if the token wasn't an indent) then return top of queue
            enqueueToken(scanner, makeToken(scanner, TOKEN_NEWLINE));
            
            return dequeueToken(scanner);
        } else {
            // Re-skip whitespace if in a grouping
            skipWhitespace(scanner);
        }
    }

    unsigned int c = advance(scanner);

    if(isAlpha(c)) return identifier(scanner);
    if(isDigit(c)) return number(scanner);

    switch(c) {
        // Switch single characters
        case '(': scanner->groupingDepth++; return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')': scanner->groupingDepth--; return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '{': scanner->groupingDepth++; return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}': scanner->groupingDepth--; return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case '[': scanner->groupingDepth++; return makeToken(scanner, TOKEN_LEFT_SQUARE);
        case ']': scanner->groupingDepth--; return makeToken(scanner, TOKEN_RIGHT_SQUARE);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case '+': return makeToken(scanner, TOKEN_PLUS);
        case '*': return makeToken(scanner, TOKEN_ASTERISK);
        case '=': return makeToken(scanner, TOKEN_EQUAL);
        case '^': return makeToken(scanner, TOKEN_CARET);
        case '%': return makeToken(scanner, TOKEN_MOD);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);
        case '|': return makeToken(scanner, TOKEN_PIPE);
        case '\\': return makeToken(scanner, TOKEN_BACK_SLASH);
        case 0xC2AC: return makeToken(scanner, TOKEN_NOT); // '¬' U+00AC, UTF-8: 0xC2AC
        case 0xE28888: return makeToken(scanner, TOKEN_IN); // '∈' U+2208, UTF-8: 0xE28888
        case 0xE288A7: return makeToken(scanner, TOKEN_AND); // '∧' U+2227, UTF-8: 0xE288A7
        case 0xE288A8: return makeToken(scanner, TOKEN_OR); // '∨' U+2228, UTF-8: 0xE288A8
        case 0xE288A9: return makeToken(scanner, TOKEN_INTERSECT); // '∩' U+2229, UTF-8: 0xE288A9
        case 0xE288AA: return makeToken(scanner, TOKEN_UNION); // '∪' U+222A, UTF-8: 0xE288AA
        case 0xE28A82: return makeToken(scanner, TOKEN_SUBSET); // '⊂' U+2282, UTF-8: 0xE28A82
        case 0xE28A86: return makeToken(scanner, TOKEN_SUBSETEQ); // '⊆' U+2286, UTF-8: 0xE28A86
        case '#': return makeToken(scanner, TOKEN_HASHTAG);
        case 0xE289A0: return makeToken(scanner, TOKEN_NOT_EQUAL); // '≠' U+2260, UTF-8: 0xE289A0
        case 0xE289A4: return makeToken(scanner, TOKEN_LESS_EQUAL); // '≤' U+2264, UTF-8: 0xE289A4
        case 0xE289A5: return makeToken(scanner, TOKEN_GREATER_EQUAL); // '≥' U+2265, UTF-8: 0xE289A5
        case 0xE28692: return makeToken(scanner, TOKEN_MAPS_TO); // '→' U+2192, UTF-8: 0xE28692
        case 0xE28792: return makeToken(scanner, TOKEN_IMPLIES); // '⇒' U+21D2, UTF-8: 0xE28792
        // case 0xE28891: return makeToken(scanner, TOKEN_SUMMATION); // '∑' U+2211, UTF-8: 0xE28891
        // Switch one or two character symbols
        case '-': return makeToken(scanner, match(scanner, '>') ? TOKEN_MAPS_TO : TOKEN_MINUS);
        case ':': return makeToken(scanner, match(scanner, '=') ? TOKEN_ASSIGN : TOKEN_COLON); 
        case '/': return makeToken(scanner, match(scanner, '=') ? TOKEN_NOT_EQUAL : TOKEN_SLASH); 
        case '>': return makeToken(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '<': return makeToken(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        // Switch three character symbols
        case '.':
            if (match(scanner, '.')) {
                if (match(scanner, '.')) {
                    return makeToken(scanner, TOKEN_ELLIPSIS);
                } else {
                    break;
                }
            }
            return makeToken(scanner, TOKEN_DOT);
        // Literals
        case '"': return string(scanner);
    }

    return errorToken(scanner, "Unexpected character");
}