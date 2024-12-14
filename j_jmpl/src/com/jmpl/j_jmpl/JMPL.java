// To run: cd to root of this repo and run:
// java -cp ./bin com.jmpl.j_jmpl.JMPL <args>
// To compile to build: cd to root of repo and run:
// javac -d ./bin ./j_jmpl/src/com/jmpl/j_jmpl/*.java

package com.jmpl.j_jmpl;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.charset.StandardCharsets;
import java.util.List;

/**
 * The main class that defines j-jmpl, an interpreter for the JMPL language written in Java.
 * Requires chcp 65001 for use with UTF-8 characters in the terminal (on Windows).
 * <p>
 * To Do: 
 * <ul>
 * <li> Implement ErrorReporter interface that passes to scanner and parser.
 * <li> Let expressions be allowed in the REPL and print their results? Or print a result for all statements.
 * <li> Add break token.
 * </ul>
 * <p>
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */
public class JMPL {
    /** Default character set used by the interpreter. */
    static final Charset DEFAULT_CHARSET = StandardCharsets.UTF_8;

    /** Boolean to ensure code which contains an error is not executed. */
    static boolean hadError = false;
    /** Boolean to ensure code which contains a runtime error is exited. */
    static boolean hadRuntimeError = false;

    private static Interpreter interpreter = new Interpreter();

    public static void main(String[] args) throws IOException {
        if(args.length > 1) {
            // Argument error
            System.out.println("Usage: JMPL [script]");
            System.exit(64); // Command line usage error
        } else if(args.length == 1) {
            // Run a file
            runFile(args[0]);
        } else {
            // REPL
            runPrompt();
        }
    }

    /** 
     * Runs a file from a given path.
     * 
     * @param  path        the path of the file to be run
     * @throws IOException if an I/O error occurs
     */
    private static void runFile(String path) throws IOException {
        byte[] bytes = Files.readAllBytes(Paths.get(path));
        String source = new String(bytes, DEFAULT_CHARSET);

        run(source);
        
        // Indicate an error in the exit code
        if (hadError) System.exit(65); // Data format error
        if (hadRuntimeError) System.exit(70); // Software error
    }

    /**
     * Runs code in the terminal as a REPL (Read-evaluate-print loop).
     * 
     * @throws IOException if an I/O error occurs
     */
    private static void runPrompt() throws IOException {
        // For some reason, doesn't support certain unicode characters
        InputStreamReader input = new InputStreamReader(System.in, DEFAULT_CHARSET);
        BufferedReader reader = new BufferedReader(input);

        // Infinite REPL
        for(;;) {
            System.out.print("> ");
            String line = reader.readLine();

            // Break loop on null line (^D on Linux, ^C on Windows)
            if(line == null) break;

            run(line);

            // Reset error flag
            hadError = false;
        }
    }

    /**
     * Runs a given source-code string.
     * 
     * @param source a byte array as a String containing the source code
     */
    private static void run(String source) {
        Scanner scanner = new Scanner(source);
        List<Token> tokens = scanner.scanTokens();
        Parser parser = new Parser(tokens);
        List<Stmt> statements = parser.parse();

        // Stop if there is a syntax error
        if(hadError) return;

        Resolver resolver = new Resolver(interpreter);
        resolver.resolve(statements);

        // Stop if there is a resolution error
        if(hadError) return;

        interpreter.interpret(statements);
    }

    //#region Error Handling

    /**
     * Prints an error message to the console to report syntax errors at a given line.
     * 
     * @param line    the line number where the error occured
     * @param type    the type of error
     * @param message the message detailing the error
     */
    static void error(int line, ErrorType type, String message) {
        report(line, type, "", message);
    }

    /**
     * Helper function for the {@link #error(int, String)} method. Prints the error message to the console.
     * Ensures code is still scanned but not executed if any errors are detected.
     * 
     * @param line    the line number where the error occured
     * @param type    the type of error
     * @param where   where the error occured
     * @param message the message detailing the error
     */
    private static void report(int line, ErrorType type, String where, String message) {
        System.err.println("[line " + line + "] " + type.getName() + where + ": " + message + ".");
        hadError = true;
    }
    
    /**
     * Overload of {@link #error(int, String)} to report an error at a given token.
     * 
     * @param token   the error token
     * @param type    the type of error
     * @param message the error message
     */
    static void error(Token token, ErrorType type, String message) {
        if(token.type == TokenType.EOF) {
            report(token.line, type, " at end", message);
        } else {
            report(token.line, type, " at '" + token.lexeme + "'", message);
        }
    }

    /**
     * Prints a runtime error to the console.
     * 
     * @param error a {@link RuntimeError} 
     */
    static void runtimeError(RuntimeError error) {
        System.err.println("[line " + error.token.line + "] " + error.type.getName() + ": " + error.getMessage() + ".");
        hadRuntimeError = true;
    } 

    //#endregion
}