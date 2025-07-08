clear
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B ./build/c_jmpl
cmake --build ./build/c_jmpl
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./build/c_jmpl/v0-2-0 ./examples/maps.jmpl
clear
cat valgrind-out.txt