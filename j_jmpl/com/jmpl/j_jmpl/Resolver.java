package com.jmpl.j_jmpl;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

/**
 * Resolver class for j-jmpl. Used for variable resolution
 * 
 * @author Joel Luckett
 * @version 0.1
 */
class Resolver implements Expr.Visitor<Void>, Stmt.Visitor<Void> {
    private final Interpreter interpreter;
    /** Keep track of current scopes. */
    private final Stack<Map<String, Boolean>> scopes = new Stack<>();
    /** Keep track fo the current function scope */
    private FunctionType currentFunction = FunctionType.NONE;

    Resolver(Interpreter interpreter) {
        this.interpreter = interpreter;
    }

    private enum FunctionType {
        NONE,
        FUNCTION
    }

    /**
     * Walk through a list of statements and resolve each one.
     * 
     * @param statements the list of statements
     */
    void resolve(List<Stmt> statements) {
        for(Stmt statement : statements) {
            resolve(statement);
        }
    }

    private void resolve(Stmt stmt) {
        stmt.accept(this);
    }

    private void resolve(Expr expr) {
        expr.accept(this);
    }

    /**
     * Resolve a function.
     * 
     * @param function the function to resolve
     * @param type     the type of function to resolve
     */
    private void resolveFunction(Stmt.Function function, FunctionType type) {
        FunctionType enclosingFunction = currentFunction;
        currentFunction = type;

        beginScope();
        for(Token param : function.params) {
            declare(param);
            define(param);
        }
        resolve(function.body);
        endScope();

        currentFunction = enclosingFunction;
    }

    /**
     * Create a new scope block.
     */
    private void beginScope() {
        scopes.push(new HashMap<String, Boolean>());
    }

    /**
     * Ends the current scope block.
     */
    private void endScope() {
        scopes.pop();
    }

    /**
     * Declare a new variable in the current scope.
     * 
     * @param name the token of the variable's identifier
     */
    private void declare(Token name) {
        if(scopes.isEmpty()) return;

        Map<String, Boolean> scope = scopes.peek();
        if(scope.containsKey(name.lexeme)) {
            JMPL.error(name, ErrorType.VARIABLE, "Already a variable with this name in this scope");
        }

        // False indicates the variable is 'not ready'
        scope.put(name.lexeme, false);
    }

    /**
     * Define a variable in the current scope.
     * 
     * @param name the token of the variable's identifier
     */
    private void define(Token name) {
        if(scopes.isEmpty()) return;

        scopes.peek().put(name.lexeme, true);
    }

    /**
     * Resolve the a local variable.
     * 
     * @param expr the expression with the variable
     * @param name the token of the variable's identifier
     */
    private void resolveLocal(Expr expr, Token name) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            if(scopes.get(i).containsKey(name.lexeme)) {
                interpreter.resolve(expr, scopes.size() - 1 - i);
                return;
            }
        }
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        beginScope();
        resolve(stmt.statements);
        endScope();

        return null;
    }

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt, FunctionType.FUNCTION);

        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        resolve(stmt.condition);
        resolve(stmt.thenBranch);

        if(stmt.elseBranch != null) resolve(stmt.elseBranch);

        return null;
    }

    @Override
    public Void visitOutputStmt(Stmt.Output stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        if(currentFunction == FunctionType.NONE) {
            JMPL.error(stmt.keyword, ErrorType.RETURN, "Can't return from top-level code");
        }

        if(stmt.value != null) {
            resolve(stmt.value);
        }

        return null;
    }

    @Override
    public Void visitLetStmt(Stmt.Let stmt) {
        declare(stmt.name);

        if(stmt.initialiser != null) {
            resolve(stmt.initialiser);
        }

        define(stmt.name);

        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        resolve(stmt.condition);
        resolve(stmt.body);

        return null;
    }


    @Override
    public Void visitAssignExpr(Expr.Assign expr) {
        resolve(expr.value);
        resolveLocal(expr, expr.name);

        return null;
    }

    @Override
    public Void visitBinaryExpr(Expr.Binary expr) {
        resolve(expr.left);
        resolve(expr.right);

        return null;
    }

    @Override
    public Void visitCallExpr(Expr.Call expr) {
        resolve(expr.callee);

        for(Expr argument : expr.arguments) {
            resolve(argument);
        }

        return null;
    }

    @Override
    public Void visitGroupingExpr(Expr.Grouping expr) {
        resolve(expr.expression);
        return null;
    }

    @Override
    public Void visitLiteralExpr(Expr.Literal expr) {
        return null;
    }

    @Override
    public Void visitLogicalExpr(Expr.Logical expr) {
        resolve(expr.left);
        resolve(expr.right);

        return null;
    }

    @Override
    public Void visitSequenceOpExpr(Expr.SequenceOp expr) {
        resolve(expr.upper);
        resolve(expr.lower);
        resolve(expr.summand);

        return null;
    }

    @Override
    public Void visitUnaryExpr(Expr.Unary expr) {
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitVariableExpr(Expr.Variable expr) {
        // Make sure variable can't be referenced in its own initialiser
        if(!scopes.isEmpty() && scopes.peek().get(expr.name.lexeme) == Boolean.FALSE) {
            JMPL.error(expr.name, ErrorType.VARIABLE, "Can't read local variable in its own initialiser");
        }

        resolveLocal(expr, expr.name);
        
        return null;
    }
}
