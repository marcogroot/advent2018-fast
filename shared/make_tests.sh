#!/bin/bash
# usage: shared/make_tests.sh <dir-of-aoc2018-inputs>
# Expects <dir>/day-NN/input/<person>.txt (layout of github.com/badouralix/adventofcode-2018).
# Copies inputs into shared/tests/, normalizes the trailing newline, and records the original x86 code's answers as .expected.
source "$(dirname "$0")/common.sh"
SRC=${1:?usage: $0 <inputs-dir>}
BIN=$("$SHARED/build.sh" original --x86) || exit 1
cd "$SHARED"
mkdir -p tests
for d in $(seq -w 1 25); do
  mkdir -p tests/day$d
  for f in "$SRC"/day-$d/input/*.txt; do
    n=$(basename "$f" .txt); case $n in example*) continue;; esac
    cp "$f" tests/day$d/$n.txt
    [ "$(tail -c1 tests/day$d/$n.txt | xxd -p)" = 0a ] || printf '\n' >> tests/day$d/$n.txt
    out=$(perl -e 'alarm(20);exec(@ARGV)' "$BIN" $((10#$d)) tests/day$d/$n.txt -n 1 2>/dev/null)
    if [ -z "$out" ] || [ $(echo "$out" | grep -c Part) -lt 1 ]; then echo "drop day$d/$n"; rm tests/day$d/$n.txt
    else echo "$out" > tests/day$d/$n.expected; fi
  done
done
find tests -name "*.txt" | wc -l
