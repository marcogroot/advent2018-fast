#!/bin/bash
# usage: shared/bench.sh <solution> [--x86] [--rounds N]
# Verifies, then benchmarks every day (best min across N full rounds, default 3).
# Writes <solution>/results.tsv (baselines original/original-x86 go to shared/results/).
# Run on an otherwise idle machine.
set -e
source "$(dirname "$0")/common.sh"
SOL=$(solution_dir "$1"); shift
NAME=$(basename "$SOL"); X86=""; ROUNDS=3
while [ $# -gt 0 ]; do
  case "$1" in
    --x86) X86="--x86"; NAME="$NAME-x86";;
    --rounds) ROUNDS=$2; shift;;
  esac
  shift
done
BIN=$("$SHARED/build.sh" "$(basename "$SOL")" $X86)
"$SHARED/verify.sh" "$BIN"
cd "$SHARED"
TMP=$(mktemp -d)
for r in $(seq 1 $ROUNDS); do "$BIN" all -q 2> "$TMP/$r.txt"; done
case "$NAME" in
  original|original-x86) OUTF="$SHARED/results/$NAME.tsv";;
  *-x86) OUTF="$SOL/results-x86.tsv";;
  *) OUTF="$SOL/results.tsv";;
esac
{
  echo "# solution: $NAME"
  echo "# machine: $(sysctl -n machdep.cpu.brand_string), $(sw_vers -productName) $(sw_vers -productVersion)"
  echo "# date: $(date +%Y-%m-%d)"
  echo "# rounds: $ROUNDS"
  echo -e "day\tmin_us\tmedian_us"
  for d in $(seq 1 25); do
    dd=$(printf %02d $d)
    grep -h "^Day $dd " "$TMP"/*.txt | awk -v d=$d '{ if (min=="" || $4+0<min+0) min=$4; if (med=="" || $7+0<med+0) med=$7 } END { printf "%d\t%s\t%s\n", d, min, med }'
  done
} > "$OUTF"
rm -rf "$TMP"
echo "wrote $OUTF"
