package com.jmpl.j_jmpl;


/**
 * Return class that extends RuntimeException. Treats return calls as excpetions so they can be thrown up the call stack.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
public class Return extends RuntimeException {
    final Object value;

    Return(Object value) {
        super(null, null, false, false);
        this.value = value;
    }
}
