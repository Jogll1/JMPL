# <img src="assets/JMPLMascot.png" height="24px"/> JMPL

This repository contains the JMPL interpreter (c_jmpl), example programs, and documentation.

JMPL has support for UTF-8 encoding and many operators have Unicode alternatives. It is recommended to use a text-editor with Unicode capabilities or a plugin that allows you to easily insert Unicode characters. If using a Windows terminal, the code page should be changed to `65001` to properly display any Unicode character that may be outputted.

## Running the Interpreter
The current version of the interpreter is c_jmpl v0.2.2.

### Prerequisites
- CMake (Tested with version 4.1.0)
- A C compiler (Tested with gcc 13.2.0)

### Build
To build the interpreter, cd to the root of this repository and run the commands: \
`cmake -S . -B ./build` to set up CMake, and \
`cmake --build ./build` to build.

On Windows you may need to specify the generator: \
`cmake -G "MinGW Makefiles" -S . -B .\build`

### Run
To run the interpreter, run the exe:
- Windows:
  `./build/v0-2-2.exe path/to/file.jmpl`
- Linux/Mac:
  `./build/v0-2-2 path/to/file.jmpl`

Running the interpreter with no source file will start the in-terminal REPL.

## Third-Party Code
List of libraries used in this project:
- <a href="https://github.com/cavaliercoder/c-stringbuilder">c-stringbuilder<a> by cavaliercodernk

## Credits
Implementation of the v0.1.0 interpreter was based off the book Crafting Interpreters by Bob Nystrom. 

Code optimisations and GC fix inspired by <a href="https://github.com/tung/clox">tung's clox</a>.

## See Also
Check out my <a href="https://jogll1.github.io/blog.html">blog</a> for updates and news about JMPL.
