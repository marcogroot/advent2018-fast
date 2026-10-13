#!/bin/bash
# usage: shared/test.sh <solution> [day ...]
#
# The one command agents need: build, verify against the original's answers, benchmark.
#   With days:    builds just those days (other days linked from original/, so parallel agents
#                 can't break each other), verifies them, and benchmarks each against original
#                 back to back. Use this while iterating.
#   Without days: full run. Builds everything, verifies all 25 days, benchmarks 3 full rounds,
#                 writes <solution>/results.tsv and prints it next to the original baselines only.
set -e
source "$(dirname "$0")/common.sh"
SOL=$(solution_dir "$1"); shift
NAME=$(basename "$SOL")
ORIG=$("$SHARED/build.sh" original)

if [ $# -eq 0 ]; then
  "$SHARED/bench.sh" "$NAME"
  python3 "$SHARED/compare.py" --only "$NAME"
  exit 0
fi

ms() { sed -E 's/.*min +([0-9.]+) us.*/\1/'; }
cd "$SHARED"
fail=0
printf "\n%-5s %14s %14s %9s\n" "Day" "original" "$NAME" "speedup"
for d in "$@"; do
  dd=$(printf %02d $((10#$d)))
  BIN=$("$SHARED/bday.sh" "$NAME" $dd)
  if ! out=$("$SHARED/verify.sh" "$BIN" $dd); then
    echo "$out"; fail=1; printf "%-5s %14s\n" "$dd" "FAILED verify"; continue
  fi
  a=$("$ORIG" $((10#$d)) -q 2>&1 | ms); b=$("$BIN" $((10#$d)) -q 2>&1 | ms)
  a2=$("$ORIG" $((10#$d)) -q 2>&1 | ms); b2=$("$BIN" $((10#$d)) -q 2>&1 | ms)
  a=$(echo "$a $a2" | awk '{print ($1<$2)?$1:$2}'); b=$(echo "$b $b2" | awk '{print ($1<$2)?$1:$2}')
  printf "%-5s %11s us %11s us %8.1fx   (verify: pass)\n" "$dd" "$a" "$b" "$(echo "$a $b" | awk '{print $1/$2}')"
done
exit $fail
