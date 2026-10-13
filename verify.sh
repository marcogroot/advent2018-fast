#!/bin/bash
# usage: ./verify.sh <binary> [day...]   — compares against original-code outputs on all test inputs
cd "$(dirname "$0")"
BIN=$1; shift
DAYS=${@:-$(seq 1 25)}; DAYS=$(echo $DAYS)
fail=0
for d in $DAYS; do
  dd=$(printf %02d $((10#$d)))
  for f in tests/day$dd/*.txt; do
    exp=${f%.txt}.expected
    got=$(perl -e "alarm(20);exec(@ARGV)" "$BIN" $((10#$d)) "$f" -n 1 2>/dev/null)
    if [ "$got" != "$(cat $exp)" ]; then echo "FAIL day$dd $(basename $f)"; diff <(echo "$got") $exp | head -4; fail=1; fi
  done
done
[ $fail = 0 ] && echo "ALL PASS ($DAYS)"
exit $fail
