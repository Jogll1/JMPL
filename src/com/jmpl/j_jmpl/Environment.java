package com.jmpl.j_jmpl;

import java.util.HashMap;
import java.util.Map;

/**
 * Environment class for j-jmpl. Stores variables that are being used by the program as key-value pairs in a HashMap.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
public class Environment {
    /** This environment's enclosing environment (higher scope). */
    final Environment enclosing;
    /** Map to store all variables as name-value pairs. */
    private final Map<String, Object> values = new HashMap<>();

    Environment() {
        enclosing = null;
    }

    Environment(Environment enclosing) {
        this.enclosing = enclosing;
    }

    /**
     * Gets the value of a stored variable by its name. Throws an error if variable is undefined.
     * 
     * @param name the token of the variable to get
     * @return     the value of the variable if it exists
     */
    Object get(Token name) {
        if(values.containsKey(name.lexeme)) {
            return values.get(name.lexeme);
        }

        // If there is an enclosing scope, get the variable from that if it cannot be found here
        if(enclosing != null) return enclosing.get(name);

        throw new RuntimeError(name, ErrorType.VARIABLE, "Undefined variable '" + name.lexeme + "'");
    }

    /**
     * Assigns a value to a stored variable.
     * 
     * @param name  the token of the variable to assign to
     * @param value the value to be assigned to the variable
     */
    void assign(Token name, Object value) {
        if(values.containsKey(name.lexeme)) {
            values.put(name.lexeme, value);
            return;
        }

        // If there is an enclosing scope, try to assign to a variable in there if it cannot be found here
        if(enclosing != null) {
            enclosing.assign(name, value);
            return;
        }

        throw new RuntimeError(name, ErrorType.VARIABLE, "Undefined variable '" + name.lexeme + "'");
    }

    /**
     * Defines a new variable by binding a name to a value and adding it to the map.
     * 
     * @param name  the name of the variable
     * @param value the value of the variable
     */
    void define(Token name, Object value) {
        // Only check if this scope defines the variable already
        if(!values.containsKey(name.lexeme)) {
            values.put(name.lexeme, value);
            return;
        }

        throw new RuntimeError(name, ErrorType.IDENTIFIER, "Identifier '" + name.lexeme + "' already defined in this scope");
    }

    /**
     * Defines a new native variable by binding a name to a value and adding it to the map.
     * 
     * @param name  the name of the native variable
     * @param value the value of the native variable
     */
    void defineNative(String name, Object value) {
        values.put(name, value);
    }
}
