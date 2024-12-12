package com.jmpl.j_jmpl;

import java.util.List;

/**
 * Callable interface for j-jmpl. 
 * 
 * @author Joel Luckett
 * @version 0.1
 */
interface JmplCallable {
    // Arity means number of arguments taken by a function
    int arity();

    Object call(Interpreter interpreter, List<Object> arguments);
}
