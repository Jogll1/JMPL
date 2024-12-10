package com.jmpl.j_jmpl;

import java.util.List;

/**
 * Interpreter class for j-jmpl. Uses the Visitor pattern. 
 * Uses {@link java.lang.Object} to store values as dynamic types (for now).
 * 
 * @author Joel Luckett
 * @version 0.1
 */
public class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {
    /** The current environment the interpreter is in. */
    private Environment environment = new Environment();

    void interpret(List<Stmt> statements) {
        try {
            for(Stmt statement : statements) {
                execute(statement);
            }
        } catch(RuntimeError e) {
            JMPL.runtimeError(e);
        } 
    }

    @Override
    public Object visitBinaryExpr(Expr.Binary expr) {
        // Evaluate operands of the expression
        Object left = evaluate(expr.left);
        Object right = evaluate(expr.right);

        // Apply the correct operation to the operands
        switch(expr.operator.type) {
            case TokenType.GREATER:
                checkNumberOperands(expr.operator, left, right);
                return (double)left > (double)right;
            case TokenType.GREATER_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double)left >= (double)right;
            case TokenType.LESS:
                checkNumberOperands(expr.operator, left, right);
                return (double)left < (double)right;
            case TokenType.LESS_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double)left <= (double)right;
            case TokenType.MINUS:
                checkNumberOperands(expr.operator, left, right);
                return (double)left - (double)right;
            case TokenType.PLUS:
                // Number addition
                if(left instanceof Double && right instanceof Double) {
                    return (double)left + (double)right;
                }

                // String concatenation
                if(left instanceof String || right instanceof String) {
                    return stringify(left) + stringify(right);
                }

                throw new RuntimeError(expr.operator, ErrorType.TYPE, "Invalid operand type(s)");
            case TokenType.ASTERISK:
                checkNumberOperands(expr.operator, left, right);
                return (double)left * (double)right;
            case TokenType.SLASH:
                if(isZero(right)) throw new RuntimeError(expr.operator, ErrorType.ZERO_DIVISION, "Division by 0");

                checkNumberOperands(expr.operator, left, right);
                return (double)left / (double)right;
            case TokenType.NOT_EQUAL: return !isEqual(left, right);
            case TokenType.EQUAL_EQUAL: return isEqual(left, right);
            default:
                break;
        }

        // Unreachable
        return null;
    }

    @Override
    public Object visitGroupingExpr(Expr.Grouping expr) {
        return evaluate(expr.expression);
    }

    @Override
    public Object visitLiteralExpr(Expr.Literal expr) {
        return expr.value;
    }

    @Override
    public Object visitUnaryExpr(Expr.Unary expr) {
        // Evaluate operand expression
        Object right = evaluate(expr.right);

        // Apply the correct operation to the operand
        switch(expr.operator.type) {
            case TokenType.MINUS:
                checkNumberOperands(expr.operator, right);
                return -(double)right;
            case TokenType.NOT:
                return !isTruthful(right);
            // Should be unreachable
            default:
                break;
        }

        // Unreachable
        return null;
    }

    @Override
    public Object visitVariableExpr(Expr.Variable expr) {
        return environment.get(expr.name);
    }

    /**
     * Checks if the operand for an operator is a number. Used for error detection when casting types.
     * 
     * @param operator the operator token
     * @param operand  the operand being checked
     */
    private void checkNumberOperands(Token operator, Object operand) {
        // Check if operand is a number
        if(operand instanceof Double) return;

        // If not, throw an error
        throw new RuntimeError(operator, ErrorType.TYPE, "Invalid operand type(s).");
    }

    /**
     * Checks if the operands for an operator are all numbers. Used for error detection when casting types.
     * Overloads {@link #checkNumberOperands(Token, Object)} to accept multiple operands.
     * 
     * @param operator  the operator token
     * @param operands  the operands being checked
     */
    private void checkNumberOperands(Token operator, Object... operands) {
        // Check if any operand is not a number
        for(Object operand : operands) {
            if(!(operand instanceof Double)) throw new RuntimeError(operator, ErrorType.TYPE, "Operands must be numbers");
        }
    }

    /**
     * Checks if an object is 'truthful' or 'truthy'. Determines the boolean state of an object, not just if it is a Boolean type.
     * <p>
     * Returns false if object is null, 0, false, or an empty string. Returns true otherwise.
     * 
     * @param object the object whose truth value is being determined
     * @return       the truth value of the object
     */
    private boolean isTruthful(Object object) {
        // What cases are false
        if(object == null) return false;
        if(object instanceof String && ((String)object).isEmpty()) return false;
        if(isZero(object)) return false;
        if(object instanceof Boolean) return (boolean)object;
        
        // Everything else is true
        return true;
    }

    /**
     * Checks if two objects are equal to each other.
     * 
     * @param a first object
     * @param b second object
     * @return  if a and b are equal
     */
    private boolean isEqual(Object a, Object b) {
        if(a == null && b == null) return true;
        if(a == null) return false;

        return a.equals(b);
    }
    
    /**
     * Formats an object into a string.
     * 
     * @param object the object to be converted to a string
     * @return       the stringified value
     */
    private String stringify(Object object) {
        if(object == null) return "null";

        if(object instanceof Double) {
            String text = object.toString();
            
            // Truncate terminating zero
            text = truncateZeros(text);

            return text;
        }

        return object.toString();
    }

    /**
     * Removes the '.0' at the end of a number casted to a string.
     *  
     * @param text th input string
     * @return     the truncated string
     */
    private String truncateZeros(String text) {
        String s = text;

        if(text.endsWith(".0")) {
            s = s.substring(0, s.length() - 2);
        }

        return s;
    }
    
    /**
     * Returns true if an object is equal to 0.
     * 
     * @param object the input object
     * @return       whether the object is zero
     */
    private boolean isZero(Object object) {
        return object instanceof Double && (Double)object == 0;
    }

    /**
     * Helper method that sends an expression back into the interpreter's Expr visitor.
     * 
     * @param expr input expression
     * @return     an {@link Object} of the expression
     */
    private Object evaluate(Expr expr) {
        return expr.accept(this);
    }

    /**
     * Helper method that sends a statement back into the interpreter's Stmt visitor.
     * 
     * @param statement input statement
     */
    private void execute(Stmt statement) {
        statement.accept(this);
    }

    /**
     * Execute each statement in a block in the correct environment.
     * 
     * @param statements  the list of statements in the block
     * @param environment the environment the block stores variables in
     */
    private void executeBlock(List<Stmt> statements, Environment environment) {
        Environment previous = this.environment;

        try {
            // Execute all statements in the block in the new environment
            this.environment = environment;

            for(Stmt statement : statements) {
                execute(statement);
            }
        } finally {
            // Return to the old environment
            this.environment = previous;
        }
    }

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        evaluate(stmt.expression);
        return null;
    }

    @Override
    public Void visitOutputStmt(Stmt.Output stmt) {
        Object value = evaluate(stmt.expression);
        System.out.println(stringify(value));
        return null;
    }

    @Override
    public Void visitLetStmt(Stmt.Let stmt) {
        Object value = null;

        if(stmt.initialiser != null) {
            value = evaluate(stmt.initialiser);
        }

        environment.define(stmt.name, value);
        return null;
    }

    @Override
    public Object visitAssignExpr(Expr.Assign expr) {
        Object value = evaluate(expr.value);
        environment.assign(expr.name, value);
        return value;
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        executeBlock(stmt.statements, new Environment(environment));
        return null;
    }
}
