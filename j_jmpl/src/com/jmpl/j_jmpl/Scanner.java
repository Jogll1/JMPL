package com.jmpl.j_jmpl;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Scanner class for j-jmpl. It reads the input source code character by character
 * and generate tokens from it.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
class Scanner {
    private final String source;
    private final List<Token> tokens = new ArrayList<>();

    // Scanner variables - keep track of where we are in the source code
    private int start = 0;
    private int current = 0;
    private int line = 1;

    /**
     * Map containing all reserved keywords.
     */
    private static final Map<String, TokenType> keywords;

    static {
        keywords = new HashMap<>();
        keywords.put("and",    TokenType.AND); // Alternative to '∧'
        keywords.put("or",     TokenType.OR); // Alternative to '∨'
        keywords.put("not",    TokenType.NOT); // Alternative to '¬'
        keywords.put("true",   TokenType.TRUE);
        keywords.put("false",  TokenType.FALSE);
        keywords.put("let",    TokenType.LET);
        keywords.put("null",   TokenType.NULL);
        keywords.put("if",     TokenType.IF);
        keywords.put("then",   TokenType.THEN);
        keywords.put("else",   TokenType.ELSE);
        keywords.put("while",  TokenType.WHILE);
        keywords.put("do",     TokenType.DO);
        keywords.put("Sum",    TokenType.SUMMATION); // Alternative to '∑'
        keywords.put("out",    TokenType.OUT);
        keywords.put("func",   TokenType.FUNCTION);
        keywords.put("return", TokenType.RETURN);
        keywords.put("in",     TokenType.IN); // Alternative to '∈'
    }

    Scanner (String source) {
        this.source = source;
    }

    /**
     * Loops through each character of the source code to generate tokens.
     * 
     * @return the list of tokens generated by scanning the source code
     */
    List<Token> scanTokens() {
        while(!isAtEnd()) {
            // At the beginning of a lexeme
            start = current;
            scanToken();
        }

        // Append End-Of-File token
        tokens.add(new Token(TokenType.EOF, "", null, line));
        return tokens;
    }

    /**
     * Scans a single token from the source.
     */
    private void scanToken() {
        char c = advance();

        switch (c) {
            // Switch single characters
            case '(': addToken(TokenType.LEFT_PAREN); break;
            case ')': addToken(TokenType.RIGHT_PAREN); break;
            case '{': addToken(TokenType.LEFT_BRACE); break;
            case '}': addToken(TokenType.RIGHT_BRACE); break;
            case '[': addToken(TokenType.LEFT_SQUARE); break;
            case ']': addToken(TokenType.RIGHT_SQUARE); break;
            case ',': addToken(TokenType.COMMA); break;
            case '.': addToken(TokenType.DOT); break;
            case '-': addToken(TokenType.MINUS); break;
            case '+': addToken(TokenType.PLUS); break;
            case '^': addToken(TokenType.CARET); break;
            case '%': addToken(TokenType.PERCENT); break;
            case ';': addToken(TokenType.SEMICOLON); break;
            case '|': addToken(TokenType.PIPE); break;
            case '∈': addToken(TokenType.IN); break;
            case '∧': addToken(TokenType.AND); break;
            case '∨': addToken(TokenType.OR); break;
            case '#': addToken(TokenType.HASHTAG); break;
            case '≠': addToken(TokenType.NOT_EQUAL); break;
            case '≥': addToken(TokenType.GREATER_EQUAL); break;
            case '≤': addToken(TokenType.LESS_EQUAL); break;
            case '→': addToken(TokenType.MAPS_TO); break;
            case '⇒': addToken(TokenType.IMPLIES); break;
            case '∑': addToken(TokenType.SUMMATION); break;
            // Switch one or two character symbols
            case ':': 
                addToken(match('=') ? TokenType.ASSIGN : TokenType.COLON);
                break;
            case '=':
                addToken(match('=') ? TokenType.EQUAL_EQUAL : TokenType.EQUAL);
                break;
            case '¬':
                addToken(match('=') ? TokenType.NOT_EQUAL : TokenType.NOT);
                break;
            case '>':
                addToken(match('=') ? TokenType.GREATER_EQUAL : TokenType.GREATER);
                break;
            case '<':
                addToken(match('=') ? TokenType.LESS_EQUAL : TokenType.LESS);
                break;
            // Case for / as comments are defined with // or /* */
            case '/':
                if(match('/')) {
                    // If it is a comment, keep consuming until end of the line
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if(match('*')) {
                    // If it is a multi-line comment, keep consuming until closed off
                    while ((peek() != '*' || peekNext() != '/') && !isAtEnd()) {
                        // Increment line counter manually when scanning multi-line comments
                        if(peek() == '\n') line++;
                        advance();
                    }
                } else {
                    addToken(TokenType.SLASH);
                }
                break;
            case '*': 
                if(!match('/')) {
                    // Only register an '*' if it is not closing a multi-line comment
                    addToken(TokenType.ASTERISK); break;
                }
            // Whitesapce and newlines
            case ' ':
            case '\r':
            case '\t': break;
            case '\n': line++; break;
            // Literals
            case '"': string(); break;
            default:
                if(isDigit(c)) {
                    // If the next character is a number, add a number literal
                    number();
                } else if(isAlpha(c)) {
                    // If the next character is a letter, add an identifier literal
                    identifier();
                }
                else {
                    // Else return an error
                    JMPL.error(line, ErrorType.SYNTAX, "Unexpected character: '" + c + "'");
                }
                break;
        }
    }

    /**
     * Adds an identifier to the token list. 
     */
    private void identifier() {
        while(isValidIdentifierCharacter(peek())) advance();

        // Check if the identifier matches a reserved keyword
        String text = source.substring(start, current);
        TokenType type = keywords.get(text);

        // If it doesn't match it's a user defined identifier
        if(type == null) type = TokenType.IDENTIFIER;

        addToken(type);
    }

    /**
     * Adds a number to the tokens list.
     * Should be called if a digit is detected.
     * Parses all numbers as doubles.
     */
    private void number() {
        // Does not detect leading '.'

        while (isDigit(peek())) advance();

        // Look for the fractional part
        if(peek() == '.' && isDigit(peekNext())) {
            // Consume the '.'
            advance();

            while(isDigit(peek())) advance();
        }

        // Add token by converting lexeme to its numerical value
        addToken(TokenType.NUMBER, Double.parseDouble(source.substring(start, current)));
    }

    /**
     * Adds string literals to tokens list.
     * Should be called if a '"' is detected.
     */
    private void string() {
        // Does not support escape sequences

        // Loop until at end of string or end of file
        while(peek() != '"' && !isAtEnd()) {
            // If a newline is found, increment line pointer
            if(peek() == '\n') line++;

            advance();
        }

        if(isAtEnd()) {
            JMPL.error(line, ErrorType.TYPE, "Unterminated string");
            return;
        }
        
        // Advance to the closing "
        advance();

        String value = source.substring(start + 1, current - 1);
        addToken(TokenType.STRING, value);
    }

    /**
     * Check whether to consume a character based on what character follows.
     * Used for two-character tokens that use one-character tokens within them.
     * 
     * @param expected the character that is expected to follow the current character
     * @return         whether to consume the multi character token
     */
    private boolean match(char expected) {
        if(isAtEnd()) return false;
        if(source.charAt(current) != expected) return false;

        current++;
        return true;
    }

    /**
     * Gets the next character in the source code (one character lookahead). This is the character pointed to by current.
     * 
     * @return the next character of the source or null character if at end of file
     */
    private char peek() {
        if(isAtEnd()) return '\0';
        return source.charAt(current);
    }

    /**
     * Gets the character two ahead in the source code. Used for looking ahead two spaces.
     * 
     * @return the character two ahead or null character if at end of file
     */
    private char peekNext() {
        if(current + 1 >= source.length()) return '\0';
        return source.charAt(current + 1);
    }

    /**
     * Checks if a given character is a digit.
     * 
     * @param c the character being checked
     * @return  if the given character is a digit
     */
    private boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    /**
     * Checks if a given character is a non-digit and valid for an identifier name.
     * Has support for: 
     * <ul>
     * <li> Latin script - a to z, A to Z
     * <li> Greek letters - α to ω, Α to Ω
     * <li> Underscores
     * </ul>
     * 
     * Accented letters are not supported.
     * @param c the character being checked
     * @return  if the given character is a valid non-digit
     */
    private boolean isAlpha(char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= 'α' && c <= 'ω') ||
               (c >= 'Α' && c <= 'Ω') ||
               (c == '_');
    }

    /**
     * Checks if a given character is valid for an indentifier.
     * Valid characters include all supported by {@link #isAlpha(char)} and the digits 0 to 9.
     * 
     * @param c the character being checked
     * @return
     */
    private boolean isValidIdentifierCharacter(char c) {
        return isAlpha(c) || isDigit(c);
    }

    /**
     * Simple check to see if scanner is at the end of the source code.
     * 
     * @return if the scanner is at the end of the source code file
     */
    private boolean isAtEnd() {
        return current >= source.length();
    }

    /**
     * Increments the current pointer to point at the next character in the source code
     * and returns it.
     * 
     * @return the next character in the source code
     */
    private char advance() {
        current++;
        return source.charAt(current - 1);
    }

    /**
     * Adds a token to the list of tokens with a null literal.
     * Overloaded version of the {@link #addToken(TokenType, Object)} method,
     * allowing tokens to be added without a literal.
     * 
     * @param type the type of the token to be added
     */
    private void addToken(TokenType type) {
        addToken(type, null);
    }


    /**
     * Adds a token to the list of tokens.
     * 
     * @param type    the type of the token to be added
     * @param literal the literal of the token to be added
     */
    private void addToken(TokenType type, Object literal) {
        String text = source.substring(start, current);
        tokens.add(new Token(type, text, literal, line));
    }
}
