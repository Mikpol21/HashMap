CXXFLAGS="-std=c++20 -g -O3 -march=native -fno-omit-frame-pointer -Wall -Wextra -pedantic"

CXXDEBUG="${CXXFLAGS} -fsanitize=address -fsanitize=undefined -fsanitize=leak"

if [[ $1 == "-d" ]];
then
    CXXFLAGS="${CXXDEBUG}"
fi

echo "${CXXFLAGS}"

g++ $CXXFLAGS main.cpp

objdump -d a.out | c++filt  > assembly.txt

g++ $CXXFLAGS benchmark.cpp -isystem ~/Libs/benchmark/include -Lbenchmark/build/src -lbenchmark -lpthread -o benchmark

g++ $CXXFLAGS test.cpp -o test -lgtest_main  -lgtest -lpthread