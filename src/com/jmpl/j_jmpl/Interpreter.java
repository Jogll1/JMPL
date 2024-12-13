package com.jmpl.j_jmpl;

import java.util.ArrayList;
import java.util.List;

/**
 * Interpreter class for j-jmpl. Uses the Visitor pattern. 
 * Uses {@link java.lang.Object} to store values as dynamic types (for now).
 * 
 * @author Joel Luckett
 * @version 0.1
 */
public class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {
    /** The environment that stores globals. */
    final Environment globals = new Environment();
    /** The current environment the interpreter is in. */
    private Environment environment = globals;

    Interpreter() {
        // When the interpreter is instantiated, stuff native functions into the global scope
        // So far they are anonymous classes - should probably find a better way

        // clock() - returns the current time
        globals.defineNative("clock", new JmplCallable() {
            @Override
            public int arity() { return 0; }

            @Override
            public Object call(Interpreter interpreter, List<Object> arguments) {
                return (double)System.currentTimeMillis() / 1000;
            }

            @Override
            public String toString() { return "<native fn>"; }
        });
    }

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
            // Comparison
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
            // Arithmetic
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
            // Boolean
            case TokenType.NOT_EQUAL: return !isEqual(left, right);
            case TokenType.EQUAL_EQUAL: return isEqual(left, right);
            default:
                break;
        }

        return null;
    }

    @Override
    public Object visitCallExpr(Expr.Call expr) {
        Object callee = evaluate(expr.callee);

        List<Object> arguments = new ArrayList<>();

        for(Expr argument : expr.arguments) {
            arguments.add(evaluate(argument));
        }

        // If the thing being called isn't a function
        if(!(callee instanceof JmplCallable)) {
            throw new RuntimeError(expr.paren, ErrorType.SYNTAX, "Only functions can be called");
        }

        JmplCallable function = (JmplCallable)callee;

        // Check the amount of arguments is the amount expected
        if(arguments.size() != function.arity()) {
            throw new RuntimeError(expr.paren, ErrorType.ARGUMENT, "Expected " + function.arity() + " arguments but got " + arguments.size());
        }

        return function.call(this, arguments);
    }

    /**
     * Should probably rework all the summation code it's not great. But it works!
     */
    @Override
    public Object visitSequenceOpExpr(Expr.SequenceOp expr) {
        if(expr.name.type == TokenType.SUMMATION) {
            Environment previous = this.environment;

            // Evaluate expressions
            Object upper = evaluate(expr.upper);
            Object lower;

            Token lowerVar;

            // Evaluate lower statement
            if(expr.lower instanceof Stmt.Let) {
                // If lower declares a new variable, do it in a new block
                environment = new Environment(previous);
                lower = evaluate(((Stmt.Let)expr.lower).initialiser);
                lowerVar = ((Stmt.Let)expr.lower).name;
                execute(expr.lower);
            } else {
                lower = evaluate(((Stmt.Expression)expr.lower).expression);
                lowerVar = ((Expr.Assign)((Stmt.Expression)expr.lower).expression).name;
            }

            Object summand = evaluate(expr.summand);

            // Errors
            if(!(Math.floor((Double)upper) == (Double)upper)) throw new RuntimeError(expr.name, ErrorType.SYNTAX, "Upper bound must be an integer");
            if(!(Math.floor((Double)lower) == (Double)lower)) throw new RuntimeError(expr.name, ErrorType.SYNTAX, "Lower bound must be an integer");
            if(!(summand instanceof Double) && !(summand instanceof String) && !(summand instanceof Character)) throw new RuntimeError(expr.name, ErrorType.SYNTAX, "Summand must be a number or a string");
            if((Double)lower > (Double)upper) throw new RuntimeError(expr.name, ErrorType.SYNTAX, "Lower bound must be less than or equal to the upper bound");

            // Perform the summation
            Object s;
            if(summand instanceof Double) {
                double sum = 0;
                while((Double)lower <= (Double)upper) {
                    sum += (Double)summand;
    
                    // Increment lower var and reassign it
                    environment.assign(lowerVar, (Double)environment.get(lowerVar) + 1);
                    lower = environment.get(lowerVar);
    
                    // Re-evaluate summand
                    summand = evaluate(expr.summand);
                }
                s = sum;
            } else {
                StringBuilder sum = new StringBuilder();
                while((Double)lower <= (Double)upper) {
                    sum.append(summand);
    
                    // Increment lower var and reassign it
                    environment.assign(lowerVar, (Double)environment.get(lowerVar) + 1);
                    lower = environment.get(lowerVar);
    
                    // Re-evaluate summand
                    summand = evaluate(expr.summand);
                }
                s = sum;
            }

            environment = previous;

            return s;
        }

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
    public Object visitLogicalExpr(Expr.Logical expr) {
        Object left = evaluate(expr.left);

        // Terminate early if we know the solution already
        if(expr.operator.type == TokenType.OR) {
            // OR
            if(isTruthful(left)) return left;
        } else  {
            // AND
            if(!isTruthful(left)) return left;
        }

        return evaluate(expr.right);
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
     * Checks if an object is 'truthful'. Determines the boolean state of an object, not just if it is a Boolean type.
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
    Object evaluate(Expr expr) {
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
    Object executeBlock(List<Stmt> statements, Environment environment) {
        Environment previous = this.environment;

        try {
            // Execute all statements in the block in the new environment
            this.environment = environment;

            for (int i = 0; i < statements.size(); i++) {
                Stmt statement = statements.get(i);

                // If this is the last statement, implicitly return the last statement if it is an expression
                // Recursively call if it's a block
                if(i == statements.size() - 1) {
                    if(statement instanceof Stmt.Block) {
                        return executeBlock(((Stmt.Block)statement).statements, environment);
                    } else if (statement instanceof Stmt.Expression) { 
                        return evaluate(((Stmt.Expression)statement).expression);
                    }
                }
                
                // If not, execute the statement
                execute(statement);
            }
        } finally {
            // Return to the old environment
            this.environment = previous;
        }

        return null;
    }

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        evaluate(stmt.expression);
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        JmplFunction function = new JmplFunction(stmt);

        // Variables and functions are stored in the same place
        environment.define(stmt.name, function);

        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        if(isTruthful(evaluate(stmt.condition))) {
            // If the condition is true, execute the then branch
            execute(stmt.thenBranch);
        } else if(stmt.elseBranch != null) {
            // If the condition is false and there's an else branch, execute the else branch
            execute(stmt.elseBranch);
        } 
        
        return null;
    }

    @Override
    public Void visitOutputStmt(Stmt.Output stmt) {
        Object value = evaluate(stmt.expression);
        System.out.println(stringify(value));
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        Object value = null;

        if(stmt.value != null) value = evaluate(stmt.value);

        throw new Return(value);
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
    public Void visitWhileStmt(Stmt.While stmt) {
        while(isTruthful(evaluate(stmt.condition))) {
            execute(stmt.body);
        }

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
