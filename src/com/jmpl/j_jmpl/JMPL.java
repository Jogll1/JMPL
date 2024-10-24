// To run: cd to root of this repo and run:
// java -cp ./bin com.jmpl.j_jmpl.JMPL <args>
// To compile to build: cd to root of repo and run:
// javac -d ./bin ./src/com/jmpl/j-jmpl/*.java

/**
 * The main class that defines the JMPL language.
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * Requires chcp 65001 to work on terminal (on Windows).
 * 
 * @author Joel Luckett
 * @version 0.1
 */

// To Do: Implemment ErrorReporter interface that passes to scanner and parser

package com.jmpl.j_jmpl;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.charset.StandardCharsets;
import java.util.List;

public class JMPL {
    /** Character set used by the interpreter. */
    static final Charset DEFAULT_CHARSET = StandardCharsets.UTF_8;

    /** Boolean to ensure code which contains an error is not executed. */
    static boolean hadError = false;

    public static void main(String[] args) throws IOException {
        if(args.length > 1) {
            System.out.println("Usage: JMPL [script]");
            System.exit(64); // Command line usage error
        } else if(args.length == 1) {
            runFile(args[0]);
        } else {
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

        // Test to print tokens
        for(Token token : tokens) {
            System.out.println(token);
        }
    }

    //#region Error Handling

    /**
     * Prints an error message to the console to provide information on syntax errors.
     * 
     * @param line    the line number where the error occured
     * @param message the message detailing the error
     */
    static void error(int line, String message) {
        report(line, "", message);
    }

    /**
     * Helper function for the {@link #error(int, String)} method. Prints the error message to the console.
     * Ensures code is still scanned but not executed if any errors are detected.
     * 
     * @param line    the line number where the error occured
     * @param where   the type of error
     * @param message the message detailing the error
     */
    private static void report(int line, String where, String message) {
        System.err.println("[line " + line + "] Error" + where + ": " + message);
        hadError = true;
    }

    //#endregion
}