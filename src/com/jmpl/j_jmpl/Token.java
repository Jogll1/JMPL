package com.jmpl.j_jmpl;

/**
 * Class for tokens that are interpreted by the scanner.
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
class Token {
    final TokenType type;
    final String lexeme;
    final Object literal;
    final int line;

    Token (TokenType type, String lexeme, Object literal, int line) {
        this.type = type;
        this.lexeme = lexeme;
        this.literal = literal;
        this.line = line;
    }

    public String toString() {
        return type + " " + lexeme + " " + literal;
    }
}