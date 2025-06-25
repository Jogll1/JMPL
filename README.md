# <img src="assets/JMPLMascot.png" height="24px"/> JMPL

This repository contains the JMPL interpreter (c_jmpl made in C), example programs, and documentation.

Files that contain non-ASCII character should be encoded in UTF-8. If using a Windows terminal, the code page should be changed to `65001` to properly display any Unicode character that may be outputted.

## Running the Interpreter
The current version of the interpreter is c_jmpl v0.1.1.

Currently, to run the interpreter have gcc or another C compiler installed then cd to root of the repo and run the commands:\
`gcc -I.\c_jmpl\include -o .\build\c_jmpl\v0-1-1 .\c_jmpl\src\*.c` to build the interpreter, \
`.\build\c_jmpl\v0-1-1.exe` to run the interpreter on a source file.

Running the interpreter with no source file will start the in-terminal REPL.

## Credits
Implementation of the v0.1.0 interpreter was based off the book Crafting Interpreters by Bob Nystrom.
