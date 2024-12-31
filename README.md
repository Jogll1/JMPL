# JMPL

This repository contains the JMPL interpreter and documentation.

Files that contain non-ASCII character should be encoded in UTF-8. If using a Windows terminal, the code page should be changed to `65001` to properly display any Unicode character that may be outputted.

## Running the Interpreter
Currently, to run the interpreter cd to root of the repo and run the commands:\
`javac -d ./bin ./src/com/jmpl/j_jmpl/*.java` to compile to `bin` then,\
`java -cp ./bin com.jmpl.j_jmpl.JMPL <path/to/file>` to run the interpreter on a source file.

Running the interpreter with no source file will start the in-terminal REPL.

## Credits
Implementation of the v0.1.0 interpreter was based off the book Crafting Interpreters by Bob Nystrom.
