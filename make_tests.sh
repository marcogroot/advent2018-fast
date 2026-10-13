#!/bin/bash
cd "$(dirname "$0")"
SRC=~/work/aoc2018-inputs-src
mkdir -p tests
for d in $(seq -w 1 25); do
  mkdir -p tests/day$d
  for f in $SRC/day-$d/input/*.txt; do
    n=$(basename $f .txt); case $n in example*) continue;; esac
    cp $f tests/day$d/$n.txt
    if [ $d = 11 ] || [ $d = 14 ]; then [ "$(tail -c1 tests/day$d/$n.txt | xxd -p)" = 0a ] || printf '\n' >> tests/day$d/$n.txt; fi
    out=$(perl -e 'alarm(20);exec(@ARGV)' ./build/bench-x86 $((10#$d)) tests/day$d/$n.txt -n 1 2>/dev/null)
    if [ -z "$out" ] || [ $(echo "$out" | grep -c Part) -lt 1 ]; then echo "drop day$d/$n"; rm tests/day$d/$n.txt; else echo "$out" > tests/day$d/$n.expected; fi
  done
done
find tests -name "*.txt" | wc -l
