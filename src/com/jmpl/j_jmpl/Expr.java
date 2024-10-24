/**
 * Abstract class for defining expressions (statements that conform to syntax).
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */

package com.jmpl.j_jmpl;

abstract class Expr {
    /**
     * Binary expressions (mainly infix expressions).
     */
    static class Binary extends Expr {
        final Expr left;
        final Token operator;
        final Expr right;

        Binary(Expr left, Token operator, Expr right) {
            this.left = left;
            this.operator = operator;
            this.right = right;
        }
    }
}
