#!/bin/bash
set -e
cd "$(dirname "$0")"
OUT=${1:-build/bench}
mkdir -p "$(dirname "$OUT")"
SRCS="src/bench.cpp $(ls src/day*.cpp | tr '\n' ' ')"
if [ "$ARCH" = x86 ]; then
  clang++ -arch x86_64 -O3 -march=haswell -std=c++17 -I/opt/homebrew/include -w $SRCS -o "$OUT"
else
  clang++ -O3 -mcpu=apple-m4 -std=c++17 -I/opt/homebrew/include -I/opt/homebrew/opt/simde/include -w $SRCS -o "$OUT"
fi
