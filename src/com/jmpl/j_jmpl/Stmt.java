package com.jmpl.j_jmpl;

/**
 * Abstract class for statements.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
abstract class Stmt {
    interface Visitor<R> {
        R visitExpressionStmt(Expression stmt);
        R visitOutputStmt(Output stmt);
        R visitLetStmt(Let stmt);
    }

    static class Expression extends Stmt {
        final Expr expression;

        Expression(Expr expression) {
            this.expression = expression;
        }

        @Override
        <R> R accept(Visitor<R> visitor) {
            return visitor.visitExpressionStmt(this);
        }
    }

    static class Output extends Stmt {
        final Expr expression;

        Output(Expr expression) {
            this.expression = expression;
        }

        @Override
        <R> R accept(Visitor<R> visitor) {
            return visitor.visitOutputStmt(this);
        }
    }

    static class Let extends Stmt {
        final Token name;
        final Expr initialiser;

        Let(Token name, Expr initialiser) {
            this.name = name;
            this.initialiser = initialiser;
        }

        @Override
        <R> R accept(Visitor<R> visitor) {
            return visitor.visitLetStmt(this);
        }
    }

    abstract <R> R accept(Visitor<R> visitor);
}
