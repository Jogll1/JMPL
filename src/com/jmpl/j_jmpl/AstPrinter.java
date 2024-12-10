package com.jmpl.j_jmpl;

/**
 * A way of printing abstract-syntax trees nicely for readability. Traverses an expression tree and parenthesises expressions
 * to show the order of execution.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
class AstPrinter implements Expr.Visitor<String> {
    String print(Expr expr) {
        return expr.accept(this);
    }

    @Override
    public String visitAssignExpr(Expr.Assign expr) {
        // Temporary
        return null;
    }

    @Override
    public String visitBinaryExpr(Expr.Binary expr) {
        return parenthesise(expr.operator.lexeme, expr.left, expr.right);
    }

    @Override
    public String visitGroupingExpr(Expr.Grouping expr) {
        return parenthesise("group", expr.expression);
    }

    @Override
    public String visitLiteralExpr(Expr.Literal expr) {
        if (expr.value == null) return "null";
        return expr.value.toString();
    }

    @Override
    public String visitUnaryExpr(Expr.Unary expr) {
        return parenthesise(expr.operator.lexeme, expr.right);
    }

    @Override
    public String visitVariableExpr(Expr.Variable expr) {
        // Temporary
        return null;
    }

    /**
     * Wraps a list of expressions in parentheses.
     * 
     * @param name  name of the token
     * @param exprs list of expressions to be parenthesised
     * @return      a string containing the Expr list in parentheses
     */
    private String parenthesise(String name, Expr... exprs) {
        StringBuilder builder = new StringBuilder();

        builder.append("(").append(name);

        for(Expr expr : exprs) {
            builder.append(" ");
            builder.append(expr.accept(this));
        }

        builder.append(")");

        return builder.toString();
    }
}
