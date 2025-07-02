#ifndef c_jmpl_scanner_h
#define c_jmpl_scanner_h

typedef enum {
    // Single-character tokens - some can be represented by words
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,   // ( )
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,   // { }
    TOKEN_LEFT_SQUARE, TOKEN_RIGHT_SQUARE, // [ ]
    TOKEN_COMMA, TOKEN_DOT,                // , .
    TOKEN_MINUS, TOKEN_PLUS,               // - +
    TOKEN_SLASH, TOKEN_ASTERISK,           // / *
    TOKEN_CARET, TOKEN_MOD,                // ^ %
    TOKEN_SEMICOLON, TOKEN_COLON,          // ; :
    TOKEN_PIPE, TOKEN_IN, TOKEN_HASHTAG,   // | ∈ #
    TOKEN_INTERSECT,TOKEN_UNION,           // ∩, ∪
 
    // One or two character tokens 
    // This is when a common character can be followed by another
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_ASSIGN, // = == := (≡?)
    TOKEN_NOT, TOKEN_NOT_EQUAL,                   // ¬ ¬= (≠)
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,           // > >= (≥)
    TOKEN_LESS, TOKEN_LESS_EQUAL,                 // < <= (≤)
    TOKEN_MAPS_TO, TOKEN_IMPLIES,                 // -> => (→, ⇒)

    // Literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

    // Keywords
    TOKEN_AND, TOKEN_OR, TOKEN_XOR,
    TOKEN_TRUE, TOKEN_FALSE,
    TOKEN_LET, TOKEN_NULL,
    TOKEN_IF, TOKEN_THEN, TOKEN_ELSE, TOKEN_WHILE, TOKEN_DO,
    TOKEN_SUMMATION, 
    TOKEN_OUT, TOKEN_RETURN, TOKEN_FUNCTION,

    TOKEN_NEWLINE, TOKEN_INDENT, TOKEN_DEDENT, 
    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const unsigned char* start;
    int length;
    int line;
} Token;

void initScanner(const unsigned char* source);
Token scanToken();

#endif