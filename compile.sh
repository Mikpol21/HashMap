CXXFLAGS="-std=c++20 -O3 -g -march=native -fno-omit-frame-pointer"

g++ $CXXFLAGS main.cpp

objdump -d a.out | c++filt  > assembly.txt

g++ $CXXFLAGS benchmark.cpp -isystem ~/Libs/benchmark/include -Lbenchmark/build/src -lbenchmark -lpthread -o benchmark

g++ $CXXFLAGS test.cpp -o test -lgtest_main  -lgtest -lpthread