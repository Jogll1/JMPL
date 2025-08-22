#ifndef c_jmpl_scanner_h
#define c_jmpl_scanner_h

#define MAX_INDENT_SIZE 16
#define TOKEN_QUEUE_SIZE 16 

typedef enum {
    // Character operators (all should have ASCII representations too)
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,    // ( )
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,    // { }
    TOKEN_LEFT_SQUARE, TOKEN_RIGHT_SQUARE,  // [ ]
    TOKEN_COMMA, TOKEN_DOT,                 // , . 
    TOKEN_MINUS, TOKEN_PLUS,                // - +
    TOKEN_SLASH, TOKEN_ASTERISK,            // / *
    TOKEN_EQUAL, TOKEN_BACK_SLASH,          // = '\'
    TOKEN_CARET, TOKEN_MOD,                 // ^ %
    TOKEN_SEMICOLON, TOKEN_COLON,           // ; :
    TOKEN_PIPE, TOKEN_IN, TOKEN_HASHTAG,    // | ∈ #
    TOKEN_INTERSECT, TOKEN_UNION,           // ∩ ∪
    TOKEN_SUBSET, TOKEN_SUBSETEQ,           // ⊂ ⊆
    TOKEN_FORALL, TOKEN_EXISTS,             // ∀ ∃

    TOKEN_EQUAL_EQUAL,                      // ==
    TOKEN_ASSIGN, TOKEN_ELLIPSIS,           // := ...
    TOKEN_NOT, TOKEN_NOT_EQUAL,             // ¬ ¬= (≠)
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,     // > >= (≥)
    TOKEN_LESS, TOKEN_LESS_EQUAL,           // < <= (≤)
    TOKEN_MAPS_TO, TOKEN_IMPLIES,           // -> => (→ ⇒)

    // Literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER, TOKEN_CHAR,

    // Keywords
    TOKEN_AND, TOKEN_OR, TOKEN_XOR,
    TOKEN_TRUE, TOKEN_FALSE,
    TOKEN_LET, TOKEN_NULL,
    TOKEN_IF, TOKEN_THEN, TOKEN_ELSE, 
    TOKEN_WHILE, TOKEN_DO, TOKEN_FOR,
    TOKEN_SOME, TOKEN_ARB,
    TOKEN_RETURN, TOKEN_FUNCTION,
    TOKEN_WITH,

    TOKEN_NEWLINE, TOKEN_INDENT, TOKEN_DEDENT, 
    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const unsigned char* start;
    int length;
    int line;
} Token;

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

void initScanner(Scanner* scanner, const unsigned char* source);
Token scanToken(Scanner* scanner);

#endif