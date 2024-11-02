package com.jmpl.j_jmpl;

import java.util.List;

/**
 * Parser class for j-jmpl. Takes in a list of tokens and generates the AST or detects errors and notifies the user.
 * Follows Recursive Descent Parsing.
 * Follows the precedence (highest to lowest):
 * Primary: true, false, null, literals, parentheses
 * Unary: ! - 
 * Factor: / *
 * Term: - +
 * Comparison: > >= < <=
 * Equality: == !=
 * To Do: add my other operators
 * 
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
class Parser {
    private final List<Token> tokens;
    /** Pointer to the next token to be parsed. */
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    private Expr expression() {
        return equality();
    }

    /**
     * Checks if the current token is an equality operator to be parsed.
     * It is at the bottom of the precedence level chain.
     * 
     * @return a Binary expression abstract syntax tree node for equality operations
     */
    private Expr equality() {
        Expr expr = comparison();

        // Check if the current token is an equality operator
        while(match(TokenType.NOT_EQUAL, TokenType.EQUAL_EQUAL)) {
            Token operator = previous();

            // Parse the right hand operand
            Expr right = comparison();

            // Create new Binary operator syntax tree node
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * Checks if the current token is a comparison operator to be parsed.
     * 
     * @return a Binary expression abstract syntax tree node for comparison operations
     */
    private Expr comparison() {
        Expr expr = term();

        while(match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL)) {
            Token operator = previous();

            // Parse the right hand operand
            Expr right = term();

            // Create new Binary operator syntax tree node
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * Checks if the current token is a term (addition and subtraction) operator to be parsed.
     * 
     * @return a Binary expression abstract syntax tree node for term operations
     */
    private Expr term() {
        Expr expr = factor();
    
        while (match(TokenType.MINUS, TokenType.PLUS)) {
            Token operator = previous();

            // Parse the right hand operand
            Expr right = factor();

            // Create new Binary operator syntax tree node
            expr = new Expr.Binary(expr, operator, right);
        }
    
        return expr;
    }

    /**
     * Checks if the current token is a factor (multiplication and division) operator to be parsed.
     * 
     * @return a Binary expression abstract syntax tree node for factor operations
     */
    private Expr factor() {
        Expr expr = unary();
    
        while (match(TokenType.SLASH, TokenType.ASTERISK)) {
            Token operator = previous();

            // Parse the right hand operand
            Expr right = unary();

            // Create new Binary operator syntax tree node
            expr = new Expr.Binary(expr, operator, right);
        }
    
        return expr;
    }

    /**
     * Checks if the current token is a unary (not and negation) operator to be parsed.
     * It is the top of the operation precedence level chain.
     * 
     * @return a Binary expression abstract syntax tree node for unary operations
     */
    private Expr unary() {
        if (match(TokenType.NOT, TokenType.MINUS)) {
            Token operator = previous();

            // Recursive call to parse the operand
            Expr right = unary();

            // Create new Unary operator syntax tree node
            return new Expr.Unary(operator, right);
        }
    
        return primary();
    }

    private Expr primary() {
        if (match(TokenType.FALSE)) return new Expr.Literal(false);
        if (match(TokenType.TRUE)) return new Expr.Literal(true);
        if (match(TokenType.NULL)) return new Expr.Literal(null);
    
        if (match(TokenType.NUMBER, TokenType.STRING)) {
            return new Expr.Literal(previous().literal);
        }
    
        if (match(TokenType.LEFT_PAREN)) {
            Expr expr = expression();

            // Call an error if a corresponding right parentheses is not found
            consume(TokenType.RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }
    }

    /**
     * Checks to see if the current token has any of the given types.
     * 
     * @param types a list of TokenTypes to check against
     * @return      if the current token matches to one of the given types
     */
    private boolean match(TokenType... types) {
        for(TokenType type : types) {
            if(check(type)) {
                advance(); // Consume token
                return true;
            }
        }

        return false;
    }

    /**
     * Returns true if the current token is of a given type
     * 
     * @param type the type to check
     * @return     whether current token matches the given type
     */
    private boolean check(TokenType type) {
        if(isAtEnd()) return false;
        return peek().type == type;
    }

    /**
     * Consume the current token and turn it. Increments the current token pointer
     * 
     * @return the current token
     */
    private Token advance() {
        if(!isAtEnd()) current++;
        return previous();
    }

    /**
     * Checks if we are at the end of the token list by comparing the current token to the EOF token.
     * 
     * @return if we are at the end of the token list
     */
    private boolean isAtEnd() {
        return peek().type == TokenType.EOF;
    }

    /**
     * Gets the current token.
     * 
     * @return the token at the location of the current pointer
     */
    private Token peek() {
        return tokens.get(current);
    }
    
    /**
     * Gets the previous token.
     * 
     * @return the token at the location of the current pointer - 1
     */
    private Token previous() {
        return tokens.get(current - 1);
    }
}
