#!/bin/bash
# usage: shared/verify.sh <solution|path-to-bench-binary> [day ...]
# Checks output against the original code's answers on every input in shared/tests/.
source "$(dirname "$0")/common.sh"
if [ -x "$1" ] && [ -f "$1" ]; then BIN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
else BIN=$("$SHARED/build.sh" "$1") || exit 1; fi
shift
DAYS=${@:-$(seq 1 25)}; DAYS=$(echo $DAYS)
cd "$SHARED"
fail=0; n=0
for d in $DAYS; do
  dd=$(printf %02d $((10#$d)))
  bad=0; total=0
  for f in tests/day$dd/*.txt; do
    n=$((n+1)); total=$((total+1))
    got=$(perl -e 'alarm(20);exec(@ARGV)' "$BIN" $((10#$d)) "$f" -n 1 2>/dev/null)
    exp=$(cat "${f%.txt}.expected")
    if [ "$got" != "$exp" ]; then
      bad=$((bad+1)); fail=1
      if [ $bad -le 3 ]; then
        line=$(diff <(echo "$exp") <(echo "$got") | grep -m1 '^<' | cut -c3-)
        gotl=$(diff <(echo "$exp") <(echo "$got") | grep -m1 '^>' | cut -c3-)
        echo "FAIL day$dd $(basename "$f"): expected '${line}' got '${gotl:-<no output / timeout>}'"
      fi
    fi
  done
  [ $bad -gt 0 ] && echo "FAIL day$dd: $bad/$total inputs wrong"
done
[ $fail = 0 ] && echo "ALL PASS ($n inputs, days: $DAYS)"
exit $fail
