clear
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B ./build
cmake --build ./build/
# valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes ./build/v0-2-2 ./test.jmpl
# kcachegrind callgrind.out.<pid>
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./build/v0-2-2 ./test.jmpl 
cat valgrind-out.txt