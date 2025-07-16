clear
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B ./build
cmake --build ./build/
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./build/v0-2-1 ./test.jmpl 
clear 
cat valgrind-out.txt