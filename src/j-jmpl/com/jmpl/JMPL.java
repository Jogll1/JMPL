/**
 * The main class that defines the JMPL language.
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */

package com.jmpl;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import java.util.Scanner;

public class JMPL {
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
     * Runs a file from a given path
     * 
     * @param path the path of the file to be run
     * @throws IOException if an I/O error occurs
     */
    private static void runFile(String path) throws IOException {
        byte[] bytes = Files.readAllBytes(Paths.get(path));
        run(new String(bytes, Charset.defaultCharset()));
    }

    /**
     * Runs code in the terminal as a REPL (Read-evaluate-print loop)
     * 
     * @throws IOException if an I/O error occurs
     */
    private static void runPrompt() throws IOException {
        InputStreamReader input = new InputStreamReader(System.in);
        BufferedReader reader = new BufferedReader(input);

        for(;;) {
            System.out.println("> ");
            String line = reader.readLine();
            if(line == null) break;
            run(line);
        }
    }

    /**
     * Runs a given string
     * 
     * @param source a byte array as a String containing the source code
     */
    private static void run(String source) {
        Scanner scanner = new Scanner(source);
        // List<Token> tokens = scanner.scanTokens();

        // // Test to print tokens
        // for(Token token : tokens) {
        //     System.out.println(token);
        // }
    }
}