#/bin/bash
make clean
make

#./test-csim
#./test-trans -M 32 -N 32
./test-trans -M 64 -N 64
#./test-trans -M 61 -N 67
