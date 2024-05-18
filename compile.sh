g++ -march=native -g -fno-omit-frame-pointer main.cpp
objdump -d a.out | c++filt  > assembly.txt