package com.jmpl.j_jmpl;

import java.util.Arrays;
import java.util.List;

/**
 * Function class for j-jmpl that implements JmplCallable. 
 * 
 * @author Joel Luckett
 * @version 0.1
 */
class JmplFunction implements JmplCallable {
    private final Stmt.Function declaration;
    /** Closure environment that holds onto surrounding variables where the function is defined. */
    private final Environment closure;

    JmplFunction(Stmt.Function declaration, Environment closure) {
        this.closure = closure;
        this.declaration = declaration;
    }

    @Override
    public int arity() {
        return declaration.params.size();
    }

    @Override 
    public Object call(Interpreter interpreter, List<Object> arguments) {
        // Define a scope for the function
        Environment environment = new Environment(closure);

        // Add all parameters as variables to the function scope
        for (int i = 0; i < declaration.params.size(); i++) {
            environment.define(declaration.params.get(i), arguments.get(i));
        }

        // Execute the body - its executing as a block because I don't want to make interpreter.environment package private
        Object value = null;
        try {
            value = interpreter.executeBlock(Arrays.asList(declaration.body), environment);
        } catch(Return returnValue) {
            // Interpret the statment until it catches a return statement, then return the value
            return returnValue.value;
        }

        return value;
    }

    @Override
    public String toString() {
        return "<fn " + declaration.name.lexeme + ">";
    }
}
