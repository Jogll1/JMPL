cmake_minimum_required(VERSION 3.15)
project(c_jmpl C)

# Headers
include_directories(c_jmpl/include c_jmpl/lib)

# Source files
file(GLOB SRC "c_jmpl/src/*.c")
file(GLOB_RECURSE LIB_SRC "c_jmpl/lib/*.c")

# Executable target
add_executable(v0-2-2 ${LIB_SRC} ${SRC})

# Link with math library on Unix (-lm)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(v0-2-2 PUBLIC ${MATH_LIBRARY})
endif()