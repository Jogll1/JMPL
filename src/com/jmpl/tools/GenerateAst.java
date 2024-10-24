/**
 * Script for generating abstract syntax tree types.
 * Implementation based of the book Crafting Interpreters by Bob Nystrom.
 * 
 * @author Joel Luckett
 * @version 0.1
 */

package com.jmpl.tools;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

public class GenerateAst {
    public static void main(String[] args) throws IOException {
        if(args.length != 1) {
            System.err.println("Usage: GenerateAst <output directory>");
            System.exit(64); // Command line usage error
        }

        String outputDir = args[0];

        defineAst(outputDir, "Expr", Arrays.asList(
        "Binary : Expr left, Token operator, Expr right",
             "Grouping : Expr expression",
             "Literal : Object value",
             "Unary : Token operator, Expr right"
        ));
    }
    
    /**
     * Method to create abstract syntax tree type classes.
     * 
     * @param  outputDir   the directory the file will be added to
     * @param  baseName    the name of the base class
     * @param  types       a list of types of ast that can be generated. Each element of the list in the format: 'name : parameters'
     * @throws IOException if an I/O error occurs
     */
    private static void defineAst(String outputDir, String baseName, List<String> types) throws IOException {
        String path = outputDir + "/" + baseName + ".java";
        PrintWriter writer = new PrintWriter(path, "UTF-8");
        writer.println("package com.jmpl.j_jmpl;");
        writer.println();
        writer.println("import java.util.List;");
        writer.println();
        writer.println("abstract class " + baseName + " {");

        // The AST classes
        for(String type : types) {
            String className = type.split(":")[0].trim();
            String fields = type.split(":")[1].trim();
            defineType(writer, baseName, className, fields);
        }

        writer.println("}");
        writer.close();
    }

    /**
     * Method to write types of abstract syntax tree to the base class.
     * 
     * @param writer    the write of the base class
     * @param baseName  the name of the base class
     * @param className the name of the sub class
     * @param fieldList String of the parameters of the subclass
     */
    private static void defineType(PrintWriter writer, String baseName, String className, String fieldList) {
        // Function signature
        writer.println("    static class " + className + " extends " + baseName + " {");

        // Fields
        String[] fields = fieldList.split(", ");
        for (String field : fields) {
            writer.println("        final " + field + ";");
        }

        writer.println();

        // Constructor
        writer.println("        " + className + "(" + fieldList + ") {");

        // Store parameters in fields
        for (String field : fields) {
            String name = field.split(" ")[1];
            writer.println("            this." + name + " = " + name + ";");
        }

        writer.println("        }");
        writer.println("    }");
        writer.println();
    }
}