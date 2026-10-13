#!/bin/bash
# usage: ./bday.sh NN  -> builds build/dayNN/bench using src/dayNN.cpp + frozen baseline objects for all other days
set -e
cd "$(dirname "$0")"
D=$(printf %02d $((10#$1)))
mkdir -p build/day$D
OBJS=$(ls build/base/*.o | grep -v "/day$D.o")
clang++ -O3 -mcpu=apple-m4 -std=c++17 -I/opt/homebrew/include -I/opt/homebrew/opt/simde/include -w -c src/day$D.cpp -o build/day$D/day$D.o
clang++ $OBJS build/day$D/day$D.o -o build/day$D/bench
