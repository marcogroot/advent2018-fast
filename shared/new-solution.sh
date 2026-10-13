#!/bin/bash
# usage: shared/new-solution.sh <name>
# Creates <name>/ as a fresh copy of original/src for a new agent to optimize.
set -e
source "$(dirname "$0")/common.sh"
NAME="${1%/}"
[ -n "$NAME" ] || { echo "usage: $0 <name>" >&2; exit 1; }
[ ! -e "$ROOT/$NAME" ] || { echo "error: $ROOT/$NAME already exists" >&2; exit 1; }
mkdir -p "$ROOT/$NAME/src"
cp "$ROOT"/original/src/advent2018.h "$ROOT"/original/src/day*.cpp "$ROOT/$NAME/src/"
cat > "$ROOT/$NAME/NOTES.md" <<NOTES
# $NAME

Agent/model: 
Date: $(date +%Y-%m-%d)

## Time taken

Wall clock (start to last day done): 
Total agent time / tokens (if known): 

## Per-day notes

NOTES
echo "created $ROOT/$NAME"
