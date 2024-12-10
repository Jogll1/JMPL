package com.jmpl.j_jmpl;

import java.util.ArrayList;
import java.util.List;

/**
 * Parser class for j-jmpl. Takes in a list of tokens and generates the AST or detects errors and notifies the user.
 * Follows Recursive Descent Parsing.
 * <p>
 * Follows the precedence (highest to lowest):
 * <ul>
 * <li>Primary: true, false, null, literals, parentheses
 * <li>Unary: !, - 
 * <li>Factor: /, *
 * <li>Term: -, +
 * <li>Comparison: >, >=, <, <=
 * <li>Equality: ==, !=
 * </ul>
 * To Do: add my other operators.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
class Parser {
    private static class ParseError extends RuntimeException {}

    private final List<Token> tokens;
    /** Pointer to the next token to be parsed. */
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    /**
     * Initial method to start the parser.
     * 
     * @return a list of {@link Stmt} statements
     */
    List<Stmt> parse() {
        List<Stmt> statements = new ArrayList<>();
        
        while(!isAtEnd()) {
            // Create a list of statements to be evaluated
            statements.add(declaration());
        }

        return statements;
    }

    /**
     * Starts the parser's expression precedence chain from the bottom.
     * 
     * @return an {@link Expr} expression
     */
    private Expr expression() {
        return assignment();
    }

    /**
     * Parse a declaration statement.
     * 
     * @return a {@link Stmt} statement
     */
    private Stmt declaration() {
        try {
            if(match(TokenType.LET)) return variableDeclaration();

            return statement();
        } catch (ParseError e) {
            synchronise();
            return null;
        }
    }

    /**
     * Parses a statement based on its type. So far handles expressions and output.
     * 
     * @return a {@link Stmt} statement
     */
    private Stmt statement() {
        if(match(TokenType.OUT)) return outputStatement();

        return expressionStatement();
    }

    /** 
     * Parses a statement that outputs to the console (identified by the OUT token)
     * 
     * @return a statement that performs an output
     */
    private Stmt outputStatement() {
        Expr value = expression();
        consumeSemicolon();
        return new Stmt.Output(value);
    }

    /**
     * Parses a statement that declares a new variable (identified by the LET token).
     * 
     * @return a statement that declares a new variable
     */
    private Stmt variableDeclaration() {
        Token name = consume(TokenType.IDENTIFIER, ErrorType.SYNTAX, "Expected variable name");

        Expr initialiser = null;

        // If there is no '=', initial value remains null
        if(match(TokenType.EQUAL)) {
            initialiser = expression();
        }

        consumeSemicolon();
        return new Stmt.Let(name, initialiser);
    }

    /** 
     * Parse a statement that is an expression.
     * 
     * @return a statement that is an expression
     */
    private Stmt expressionStatement() {
        Expr expr = expression();
        consumeSemicolon();
        return new Stmt.Expression(expr);
    }

    /**
     * Assign a value to an already stored variable.
     * 
     * @return an abstract syntax tree for assignment operations
     */
    private Expr assignment() {
        Expr expr = equality();

        if(match(TokenType.ASSIGN)) {
            Token equals = previous();
            Expr value = assignment();

            if(expr instanceof Expr.Variable) {
                Token name = ((Expr.Variable)expr).name;
                return new Expr.Assign(name, value);
            }

            error(equals, ErrorType.SYNTAX, "Invalid assignment target");
        }

        return expr;
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
     * It is the top of the operation precedence level chain (before primary statements).
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

    /**
     * Evaluates primary expressions (literals, boolean values, etc.).
     * 
     * @return a Binary expression abstract syntax tree node for primary operations
     */
    private Expr primary() {
        if (match(TokenType.FALSE)) return new Expr.Literal(false);
        if (match(TokenType.TRUE)) return new Expr.Literal(true);
        if (match(TokenType.NULL)) return new Expr.Literal(null);
    
        if (match(TokenType.NUMBER, TokenType.STRING)) {
            return new Expr.Literal(previous().literal);
        }

        if(match(TokenType.IDENTIFIER)) {
            return new Expr.Variable(previous());
        }
    
        if (match(TokenType.LEFT_PAREN)) {
            Expr expr = expression();

            // Call an error if a corresponding right parentheses is not found
            consume(TokenType.RIGHT_PAREN, ErrorType.SYNTAX, "Expected ')' after expression");
            return new Expr.Grouping(expr);
        }

        // Throw an error if unexpected token
        throw error(peek(), ErrorType.SYNTAX, "Expression expected");
    }

    /**
     * Checks to see if the current token is any of the given types.
     * 
     * @param types a list of {@link TokenType}s to check against
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
     * Checks a token against a desired type which is consumed if it is a match. If not, it throws an error.
     * Message does not need a full stop at the end.
     * 
     * @param type    the expected token to consume
     * @param message the potential error message if the token is not found
     * @return        the current token if it is the desired token
     * @throws error  signified by the message parameter, thrown if the desired token is not found
     */
    private Token consume(TokenType type, ErrorType errorType, String message) {
        if(check(type)) return advance();

        throw error(peek(), ErrorType.SYNTAX, message);
    }

    /**
     * Shorthand way to consume a semicolon and report potential errors.
     */
    private void consumeSemicolon() {
        consume(TokenType.SEMICOLON, ErrorType.SYNTAX, "Expected ';' after value");
    }

    /**
     * Returns true if the current token is of a given type.
     * 
     * @param type the type to check
     * @return     whether current token matches the given type
     */
    private boolean check(TokenType type) {
        if(isAtEnd()) return false;
        return peek().type == type;
    }

    /**
     * Consume the current token and returns it. Increments the current token pointer.
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

    /**
     * Shows an error to the user.
     * 
     * @param token   the token that caused an error
     * @param type    the type of error
     * @param message the error message
     * @return        a {@link ParseError}
     */
    private ParseError error(Token token, ErrorType type, String message) {
        JMPL.error(token, type, message);
        return new ParseError();
    }
    
    /**
     * Synchronises the parser by discarding tokens until we reach the start of the next statement.
     */
    private void synchronise() {
        advance();

        while(!isAtEnd()) {
            if(previous().type == TokenType.SEMICOLON) return;

            // Switch the tokens that should start a statement
            switch(peek().type) {
                case TokenType.FUNCTION:
                case TokenType.LET:
                case TokenType.FOR:
                case TokenType.IF:
                case TokenType.WHILE:
                case TokenType.OUT:
                case TokenType.RETURN:
                    return;
                default:
                    // If another token is found
                    break;
            }

            advance();
        }
    }
}