package com.jmpl.j_jmpl;

enum TokenType {
    // Single-character tokens - some can be represented by words
    // To Do: add XOR support
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,  // ( ) { }
    LEFT_SQUARE, RIGHT_SQUARE,                         // [ ]
    COMMA, DOT, MINUS, PLUS, SLASH, ASTERISK,          // , . - + / *
    CARET, PERCENT,                                    // ^ %
    SEMICOLON, COLON, PIPE, IN, HASHTAG,               // ; : | ∈ #
 
    // One or two character tokens 
    // This is when a common character can be followed by another
    // To Do: add support for unicode of these
    EQUAL, EQUAL_EQUAL,                                // = == (≡?)
    NOT, NOT_EQUAL,                                    // ¬ ¬= (≠)
    GREATER, GREATER_EQUAL,                            // > >= (≥)
    LESS, LESS_EQUAL,                                  // < <= (≤)
    MAPS_TO, IMPLIES,                                  // -> => (→, ⇒)

    // Literals
    IDENTIFIER, STRING, NUMBER,

    // Keywords
    AND, OR,
    TRUE, FALSE,
    LET, NULL,
    IF, ELSE, WHILE, FOR,
    OUT, RETURN, FUNCTION,

    EOF
}