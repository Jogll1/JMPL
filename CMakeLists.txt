cmake_minimum_required(VERSION 3.15)
project(c_jmpl C)

# Headers
include_directories(c_jmpl/include c_jmpl/lib/c-stringbuilder)

# Source files
file(GLOB SRC "c_jmpl/src/*.c")
file(GLOB_RECURSE LIB_SRC "c_jmpl/lib/c-stringbuilder/*.c")

# Executable target
add_executable(v0-2-0 ${LIB_SRC} ${SRC})