/**
 * The main class that defines the JMPL language.
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */

// To Do: Implemment ErrorReporter interface that passes to scanner and parser

package com.jmpl;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

import com.jmpl.Scanner;

public class JMPL {
    /** Boolean to ensure code which contains an error is not executed.*/
    static boolean hadError = false;

    public static void main(String[] args) throws IOException {
        if(args.length > 1) {
            System.out.println("Usage: j-jmpl [script]");
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
        run(new String(bytes, Charset.defaultCharset()));
        
        // Indicate an error in the exit code
        if (hadError) System.exit(65); // Data format error
    }

    /**
     * Runs code in the terminal as a REPL (Read-evaluate-print loop).
     * 
     * @throws IOException if an I/O error occurs
     */
    private static void runPrompt() throws IOException {
        InputStreamReader input = new InputStreamReader(System.in);
        BufferedReader reader = new BufferedReader(input);

        // Infinite REPL
        for(;;) {
            System.out.println("> ");
            String line = reader.readLine();

            // Break loop on null line (^D)
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
     * @param where   
     * @param message the message detailing the error
     */
    private static void report(int line, String where, String message) {
        System.err.println("[line ]" + line + "] Error" + where + ": " + message);
        hadError = true;
    }

    //#endregion
}