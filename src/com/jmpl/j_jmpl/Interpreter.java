package com.jmpl.j_jmpl;

/**
 * Interpreter class for j-jmpl. Uses the Visitor pattern. Will use {@link java.lang.Object} to store values as dynamic types (for now).
 * <p>
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
public class Interpreter implements Expr.Visitor<Object> {
    void interpret(Expr expression) {
        try {
            Object value = evaluate(expression);
            System.out.println(stringify(value));
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
                if(left instanceof String && right instanceof String) {
                    return (String)left + (String)right;
                }

                throw new RuntimeError(expr.operator, "Operands must be both numbers or strings.");
            case TokenType.ASTERISK:
                checkNumberOperands(expr.operator, left, right);
                return (double)left * (double)right;
            case TokenType.SLASH:
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
        throw new RuntimeError(operator, "Operand must be a number.");
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
            if(!(operand instanceof Double)) throw new RuntimeError(operator, "Operands must be numbers.");
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
        if(object instanceof Double && (Double)object == 0) return false;
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
     * Converts a value into a string.
     * 
     * @param object the object to be converted to a string
     * @return       the stringified value
     */
    private String stringify(Object object) {
        if(object == null) return "null";

        if(object instanceof Double) {
            String text = object.toString();
            
            // Truncate terminating zero
            if(text.endsWith(".0")) {
                text = text.substring(0, text.length() - 2);
            }

            return text;
        }

        return object.toString();
    }

    /**
     * Helper method that sends an expression back into the interpreter's visitor implementation.
     * 
     * @param expr input expression
     * @return     an {@link Object} of the expression
     */
    private Object evaluate(Expr expr) {
        return expr.accept(this);
    }
}
