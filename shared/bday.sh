#!/bin/bash
# usage: shared/bday.sh <solution> <day>
# Builds build/<solution>/dayNN/bench from <solution>/src/dayNN.cpp, linking every OTHER day from original/.
# Lets several agents work on different days of the same solution at once without breaking each other's builds.
set -e
source "$(dirname "$0")/common.sh"
SOL=$(solution_dir "$1")
D=$(printf %02d $((10#$2)))
"$SHARED/build.sh" original >/dev/null
OUT="$ROOT/build/$(basename "$SOL")/day$D"
mkdir -p "$OUT"
clang++ $CXXFLAGS_NATIVE -I"$SOL/src" -c "$SOL/src/day$D.cpp" -o "$OUT/day$D.o"
OBJS=$(ls "$ROOT/build/original/obj/"*.o | grep -v "/day$D.o")
clang++ $CXXFLAGS_NATIVE $OBJS "$OUT/day$D.o" -o "$OUT/bench"
echo "$OUT/bench"
