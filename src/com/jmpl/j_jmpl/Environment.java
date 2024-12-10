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
    private final Map<String, Object> values = new HashMap<>();

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

        throw new RuntimeError(name, ErrorType.VARIABLE, "Undefined variable '" + name.lexeme + "'");
    }

    /**
     * Defines a new variable by binding a name to a value and adding it to the map.
     * <p>
     * To Do: Add an error when trying to redefine an existing variable.
     * 
     * @param name  the name of the variable
     * @param value the value of the variable
     */
    void define(String name, Object value) {
        values.put(name, value);
    }
}
