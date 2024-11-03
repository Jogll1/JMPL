package com.jmpl.j_jmpl;

/**
 * Runtime error class for j-jmpl. Inherits {@link java.lang.RuntimeException} and stores token information.
 * <p>
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
public class RuntimeError extends RuntimeException {
    final Token token;
    final ErrorType type;

    RuntimeError(Token token, ErrorType type, String message) {
        super(message);
        this.token = token;
        this.type = type;
    }
}
