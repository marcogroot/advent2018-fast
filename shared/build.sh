#!/bin/bash
# usage: shared/build.sh <solution> [--x86]
# Builds build/<solution>/bench (or build/<solution>-x86/bench), recompiling only changed files.
set -e
source "$(dirname "$0")/common.sh"
SOL=$(solution_dir "$1")
NAME=$(basename "$SOL")
FLAGS=$CXXFLAGS_NATIVE
if [ "$2" = "--x86" ]; then FLAGS=$CXXFLAGS_X86; NAME="$NAME-x86"; fi
OUT="$ROOT/build/$NAME"
mkdir -p "$OUT/obj"
OBJS=()
for src in "$SHARED/bench.cpp" "$SOL"/src/day*.cpp; do
  obj="$OUT/obj/$(basename "$src" .cpp).o"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ "$SOL/src/advent2018.h" -nt "$obj" ]; then
    clang++ $FLAGS -I"$SOL/src" -c "$src" -o "$obj" &
  fi
  OBJS+=("$obj")
done
wait
relink=0
[ -f "$OUT/bench" ] || relink=1
for obj in "${OBJS[@]}"; do [ "$obj" -nt "$OUT/bench" ] && relink=1; done
[ $relink = 1 ] && clang++ $FLAGS "${OBJS[@]}" -o "$OUT/bench"
echo "$OUT/bench"
