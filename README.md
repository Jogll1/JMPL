# JMPL

This repository contains the JMPL interpreter and documentation.

## Running the Interpreter
Currently, to run the interpreter cd to root of the repo and run the commands:\
`javac -d ./bin ./src/com/jmpl/j_jmpl/*.java` to compile to `bin` then,\
`java -cp ./bin com.jmpl.j_jmpl.JMPL <path/to/file>` to run the interpreter on a source file.

Running the interpreter with no source file will start the in-terminal REPL.

## Credits
Implementation of the v0.1.0 interpreter was based off the book Crafting Interpreters by Bob Nystrom.
